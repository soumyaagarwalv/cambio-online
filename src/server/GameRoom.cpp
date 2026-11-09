#include "server/GameRoom.h"
#include "server/GameView.h"
#include "server/Protocol.h"

GameRoom::GameRoom(std::string id)
    : roomId(std::move(id)) {}

bool GameRoom::join(const std::string& name,
                    SendFunction send,
                    int& playerId,
                    std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);

    if (state) {
        error = "The game has already started; joining is closed.";
        return false;
    }
    if (names.size() >= 4) {
        error = "Room is full. Maximum 4 players.";
        return false;
    }
    if (!send) {
        error = "Missing connection callback.";
        return false;
    }

    playerId = nextPlayerId++;
    names.push_back(name.empty() ? ("Player " + std::to_string(playerId)) : name);
    connections[playerId] = Connection{names.back(), std::move(send)};

    connections[playerId].send(
        "{\"type\":\"joined\",\"roomId\":\"" + roomId +
        "\",\"playerId\":" + std::to_string(playerId) + "}"
    );

    broadcastLobbyLocked();
    return true;
}

bool GameRoom::start(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);

    if (state) {
        error = "Game has already started.";
        return false;
    }
    if (connections.empty()) {
        error = "No players are connected.";
        return false;
    }
    if (names.size() < 2) {
        error = "At least 2 players are required to start.";
        return false;
    }

    state = std::make_unique<GameState>(names);
    engine = std::make_unique<GameEngine>(*state);

    broadcastViewsLocked();
    return true;
}

bool GameRoom::handleAction(int playerId,
                            const Action& action,
                            std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);

    if (!state || !engine) {
        error = "The game has not started yet.";
        return false;
    }
    if (!connections.count(playerId)) {
        error = "Player is not connected to this room.";
        return false;
    }

    if (!engine->performAction(playerId, action, error)) {
        return false;
    }

    broadcastViewsLocked();
    engine->clearEvents();
    return true;
}

void GameRoom::disconnect(int playerId) {
    std::lock_guard<std::mutex> lock(mutex);
    connections.erase(playerId);
    // We deliberately do not mutate GameState on disconnect.
    // Reconnection/pause semantics are a later server feature.
}

void GameRoom::broadcastViewsLocked() {
    for (const auto& [playerId, connection] : connections) {
        try {
            sendViewLocked(playerId);
        } catch (...) {
            // A broken socket must not corrupt the authoritative game state.
        }
    }
}

void GameRoom::broadcastLobbyLocked() {
    const int count = static_cast<int>(names.size());
    const bool canStart = count >= 2 && !state;

    const std::string message =
        "{\"type\":\"lobby\",\"playerCount\":" +
        std::to_string(count) +
        ",\"canStart\":" + (canStart ? "true" : "false") + "}";

    for (const auto& [playerId, connection] : connections) {
        (void)playerId;
        try {
            connection.send(message);
        } catch (...) {
        }
    }
}

void GameRoom::sendViewLocked(int playerId) {
    auto it = connections.find(playerId);
    if (it == connections.end()) return;

    const GameView view = GameViewBuilder::build(*state, *engine, playerId);
    it->second.send(gameViewToJson(view));
}

bool GameRoom::isStarted() const {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<bool>(state);
}

int GameRoom::playerCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<int>(names.size());
}

std::string GameRoom::id() const {
    std::lock_guard<std::mutex> lock(mutex);
    return roomId;
}
