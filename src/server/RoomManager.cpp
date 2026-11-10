#include "server/RoomManager.h"

#include <cctype>

namespace {
// No 0/O, 1/I/L: codes are read aloud and typed on phones.
constexpr char kAlphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
constexpr std::size_t kAlphabetSize = sizeof(kAlphabet) - 1;
}  // namespace

RoomManager::RoomManager() : rng(std::random_device{}()) {}

std::string RoomManager::normalizeCode(const std::string& raw) {
    std::string out;
    for (unsigned char c : raw) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out;
}

std::string RoomManager::generateCodeLocked() {
    std::uniform_int_distribution<std::size_t> pick(0, kAlphabetSize - 1);
    for (;;) {
        std::string code;
        for (std::size_t i = 0; i < kCodeLength; ++i) code.push_back(kAlphabet[pick(rng)]);
        if (!rooms.count(code)) return code;
    }
}

std::shared_ptr<GameRoom> RoomManager::createRoom(std::string& error) {
    std::lock_guard<std::mutex> lock(mutex);
    if (rooms.size() >= kMaxRooms) {
        error = "The server is busy right now. Please try again in a few minutes.";
        return nullptr;
    }
    const std::string code = generateCodeLocked();
    auto room = std::make_shared<GameRoom>(code);
    rooms.emplace(code, room);
    return room;
}

std::shared_ptr<GameRoom> RoomManager::findRoom(const std::string& code) const {
    const std::string key = normalizeCode(code);
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(key);
    return it == rooms.end() ? nullptr : it->second;
}

void RoomManager::removeIfEmpty(const std::shared_ptr<GameRoom>& room) {
    if (!room) return;
    const std::string key = room->id();
    std::lock_guard<std::mutex> lock(mutex);
    // Lock order is always manager -> room. join() never holds the room lock
    // while calling the manager, so this cannot deadlock.
    if (!room->closeIfEmpty()) return;
    auto it = rooms.find(key);
    if (it != rooms.end() && it->second == room) rooms.erase(it);
}

std::size_t RoomManager::roomCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return rooms.size();
}
