#include "server/Protocol.h"

#include <cctype>
#include <sstream>
#include <unordered_map>

namespace {
std::unordered_map<std::string, std::string> parseFlatJson(const std::string& s) {
    std::unordered_map<std::string, std::string> m;
    std::size_t i = 0;
    auto skip = [&]() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; };
    skip();
    if (i >= s.size() || s[i] != '{') return {};
    ++i;
    while (i < s.size()) {
        skip();
        if (i < s.size() && s[i] == '}') break;
        if (i >= s.size() || s[i] != '"') return {};
        ++i;
        std::string key;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) ++i;
            key.push_back(s[i++]);
        }
        if (i >= s.size()) return {};
        ++i;
        skip();
        if (i >= s.size() || s[i] != ':') return {};
        ++i;
        skip();
        std::string value;
        if (i < s.size() && s[i] == '"') {
            ++i;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) ++i;
                value.push_back(s[i++]);
            }
            if (i >= s.size()) return {};
            ++i;
        } else {
            while (i < s.size() && s[i] != ',' && s[i] != '}') value.push_back(s[i++]);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
        }
        m[key] = value;
        skip();
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') break;
        return {};
    }
    return m;
}

int getInt(const std::unordered_map<std::string,std::string>& m,
           const std::string& key, int fallback = -1) {
    auto it = m.find(key);
    if (it == m.end()) return fallback;
    try { return std::stoi(it->second); } catch (...) { return fallback; }
}

bool getBool(const std::unordered_map<std::string,std::string>& m,
             const std::string& key, bool fallback = false) {
    auto it = m.find(key);
    if (it == m.end()) return fallback;
    return it->second == "true" || it->second == "1";
}

std::optional<ActionType> actionTypeFromString(const std::string& s) {
#define A(x) if (s == #x) return ActionType::x
    A(CONFIRM_READY);
    A(DRAW_FROM_DECK);
    A(DRAW_FROM_DISCARD);
    A(REPLACE_CARD_WITH_DRAWN);
    A(DISCARD_DRAWN_CARD);
    A(USE_POWER_AND_DISCARD_DRAWN);
    A(PEEK_OWN_CARD);
    A(PEEK_OPPONENT_CARD);
    A(BLIND_SWAP);
    A(LOOK_AND_OPTIONALLY_SWAP);
    A(LOOK_TWO_CARDS);
    A(DECIDE_SWAP);
    A(FLIP_CARD);
    A(GIVE_CARD_AFTER_FLIP);
    A(CALL_CAMBIO);
#undef A
    return std::nullopt;
}
}

std::optional<ClientCommand> parseClientCommand(const std::string& json,
                                                std::string& error) {
    error.clear();
    const auto m = parseFlatJson(json);
    if (m.empty()) {
        error = "Invalid JSON object.";
        return std::nullopt;
    }

    auto typeIt = m.find("type");
    if (typeIt == m.end()) {
        error = "Missing command type.";
        return std::nullopt;
    }

    ClientCommand cmd;
    if (typeIt->second == "join") {
        cmd.type = ClientCommand::Type::JOIN;
        cmd.roomId = m.count("roomId") ? m.at("roomId") : "default";
        cmd.playerName = m.count("name") ? m.at("name") : "Player";
        return cmd;
    }
    if (typeIt->second == "start") {
        cmd.type = ClientCommand::Type::START;
        return cmd;
    }
    if (typeIt->second == "ping") {
        cmd.type = ClientCommand::Type::PING;
        return cmd;
    }
    if (typeIt->second != "action") {
        error = "Unknown command type.";
        return std::nullopt;
    }

    cmd.type = ClientCommand::Type::ACTION;
    auto actionIt = m.find("action");
    if (actionIt == m.end()) {
        error = "Missing action.";
        return std::nullopt;
    }
    const auto actionType = actionTypeFromString(actionIt->second);
    if (!actionType.has_value()) {
        error = "Unknown action type.";
        return std::nullopt;
    }

    cmd.action.type = *actionType;
    cmd.action.position = getInt(m, "position");
    cmd.action.targetPlayer = getInt(m, "targetPlayer");
    cmd.action.targetPosition = getInt(m, "targetPosition");
    cmd.action.secondTargetPlayer = getInt(m, "secondTargetPlayer");
    cmd.action.secondTargetPosition = getInt(m, "secondTargetPosition");
    cmd.action.extraCardIndex = getInt(m, "extraCardIndex", -1);
    cmd.action.shouldSwap = getBool(m, "shouldSwap");
    return cmd;
}

std::string makeErrorMessage(const std::string& message) {
    std::string escaped;
    for (char c : message) {
        if (c == '\\' || c == '"') escaped.push_back('\\');
        escaped.push_back(c);
    }
    return "{\"type\":\"error\",\"message\":\"" + escaped + "\"}";
}

std::string makeInfoMessage(const std::string& message) {
    std::string escaped;
    for (char c : message) {
        if (c == '\\' || c == '"') escaped.push_back('\\');
        escaped.push_back(c);
    }
    return "{\"type\":\"info\",\"message\":\"" + escaped + "\"}";
}
