#include "server/RoomManager.h"

std::shared_ptr<GameRoom> RoomManager::getOrCreate(const std::string& roomId) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = rooms.find(roomId);
    if (it != rooms.end()) return it->second;

    auto room = std::make_shared<GameRoom>(roomId);
    rooms.emplace(roomId, room);
    return room;
}
