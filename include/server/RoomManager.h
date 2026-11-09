#ifndef CAMBIO_SERVER_ROOM_MANAGER_H
#define CAMBIO_SERVER_ROOM_MANAGER_H

#include "server/GameRoom.h"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class RoomManager {
private:
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<GameRoom>> rooms;

public:
    std::shared_ptr<GameRoom> getOrCreate(const std::string& roomId);
};

#endif
