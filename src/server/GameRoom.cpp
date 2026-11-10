#include "server/GameRoom.h"
#include "server/GameView.h"
#include "server/Protocol.h"

#include <sstream>

namespace {
std::string jsonEscape(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"':  out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << ' ';
                else out << static_cast<char>(c);
        }
    }
    return out.str();
}

std::string cleanName(const std::string& raw, int fallbackNumber) {
    std::string name;
    for (unsigned char c : raw) {
        if (c < 0x20) continue;     // drop control characters
        name.push_back(static_cast<char>(c));
    }
    // Trim spaces and cap the length (bytes; cut is fixed up below for UTF-8).
    while (!name.empty() && name.front() == ' ') name.erase(name.begin());
    while (!name.empty() && name.back() == ' ') name.pop_back();
    constexpr std::size_t kMax = 20;
    if (name.size() > kMax) {
        name.resize(kMax);
        // Do not leave half a multi-byte UTF-8 character at the end.
        while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80)
            name.pop_back();
        if (!name.empty() && (static_cast<unsigned char>(name.back()) & 0x80))
            name.pop_back();
    }
    if (name.empty()) name = "Player " + std::to_string(fallbackNumber + 1);
    return name;
}
}  // namespace

GameRoom::GameRoom(std::string id)
    : roomId(std::move(id)) {}

int GameRoom::hostIdLocked() const {
    return connections.empty() ? -1 : connections.begin()->first;
}

bool GameRoom::join(const std::string& name,
                    SendFunction send,
                    int& connectionId,
                    std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);

    if (closed) {
        error = "That game no longer exists.";
        return false;
    }
    if (state) {
        error = "The game has already started; joining is closed.";
        return false;
    }
    if (static_cast<int>(connections.size()) >= kMaxPlayers) {
        error = "Room is full. Maximum 4 players.";
        return false;
    }
    if (!send) {
        error = "Missing connection callback.";
        return false;
    }

    connectionId = nextConnectionId++;
    connections[connectionId] =
        Connection{cleanName(name, static_cast<int>(connections.size())), std::move(send)};

    try {
        connections[connectionId].send(
            "{\"type\":\"joined\",\"roomId\":\"" + jsonEscape(roomId) +
            "\",\"playerId\":" + std::to_string(connectionId) + "}");
    } catch (...) {
    }

    broadcastLobbyLocked();
    return true;
}

bool GameRoom::start(int connectionId, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);

    if (state) {
        error = "Game has already started.";
        return false;
    }
    if (connectionId != hostIdLocked()) {
        error = "Only the host can start the game.";
        return false;
    }
    if (connections.size() < 2) {
        error = "At least 2 players are required to start.";
        return false;
    }

    // Players still in the lobby, in join order, become player 0..n-1.
    std::vector<std::string> names;
    engineIndexOf.clear();
    for (const auto& [id, connection] : connections) {
        engineIndexOf[id] = static_cast<int>(names.size());
        names.push_back(connection.name);
    }

    state = std::make_unique<GameState>(names);
    engine = std::make_unique<GameEngine>(*state);

    // Tell each client which seat it ended up in (may differ from the id it
    // was given at join time if someone left the lobby).
    for (const auto& [id, connection] : connections) {
        try {
            connection.send("{\"type\":\"seat\",\"playerId\":" +
                            std::to_string(engineIndexOf[id]) + "}");
        } catch (...) {
        }
    }

    broadcastViewsLocked();
    return true;
}

bool GameRoom::handleAction(int connectionId,
                            const Action& action,
                            std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!state || !engine) {
        error = "The game has not started yet.";
        return false;
    }
    const auto seat = engineIndexOf.find(connectionId);
    if (seat == engineIndexOf.end() || !connections.count(connectionId)) {
        error = "Player is not connected to this room.";
        return false;
    }

    if (!engine->performAction(seat->second, action, error)) {
        return false;
    }

    broadcastViewsLocked();
    engine->clearEvents();
    return true;
}

void GameRoom::disconnect(int connectionId) {
    std::lock_guard<std::mutex> lock(mutex);
    if (connections.erase(connectionId) == 0) return;

    // Before the game starts, a leaving player simply frees their seat.
    // After it starts we deliberately do not mutate GameState on disconnect;
    // reconnection/pause semantics are a later server feature.
    if (!state) broadcastLobbyLocked();
}

bool GameRoom::closeIfEmpty() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connections.empty()) return false;
    closed = true;
    return true;
}

void GameRoom::broadcastViewsLocked() {
    for (const auto& [connectionId, connection] : connections) {
        (void)connection;
        try {
            sendViewLocked(connectionId);
        } catch (...) {
            // A broken socket must not corrupt the authoritative game state.
        }
    }
}

void GameRoom::broadcastLobbyLocked() {
    const int count = static_cast<int>(connections.size());
    const int hostId = hostIdLocked();

    std::string players = "[";
    bool first = true;
    for (const auto& [id, connection] : connections) {
        (void)id;
        if (!first) players += ",";
        first = false;
        players += "\"" + jsonEscape(connection.name) + "\"";
    }
    players += "]";

    for (const auto& [id, connection] : connections) {
        const bool isHost = (id == hostId);
        const bool canStart = isHost && count >= 2 && !state;
        const std::string message =
            "{\"type\":\"lobby\",\"roomId\":\"" + jsonEscape(roomId) +
            "\",\"playerCount\":" + std::to_string(count) +
            ",\"players\":" + players +
            ",\"host\":" + (isHost ? "true" : "false") +
            ",\"canStart\":" + (canStart ? "true" : "false") + "}";
        try {
            connection.send(message);
        } catch (...) {
        }
    }
}

void GameRoom::sendViewLocked(int connectionId) {
    auto it = connections.find(connectionId);
    auto seat = engineIndexOf.find(connectionId);
    if (it == connections.end() || seat == engineIndexOf.end()) return;

    const GameView view = GameViewBuilder::build(*state, *engine, seat->second);
    it->second.send(gameViewToJson(view));
}

bool GameRoom::isStarted() const {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<bool>(state);
}

int GameRoom::playerCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(connections.size());
}

std::string GameRoom::id() const {
    std::lock_guard<std::mutex> lock(mutex);
    return roomId;
}
