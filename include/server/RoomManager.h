#ifndef CAMBIO_SERVER_ROOM_MANAGER_H
#define CAMBIO_SERVER_ROOM_MANAGER_H

#include "server/GameRoom.h"
#include <cstddef>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>

// Owns every active game room on this server. All methods are thread-safe.
class RoomManager {
public:
    static constexpr std::size_t kMaxRooms = 500;
    static constexpr std::size_t kCodeLength = 6;

private:
    mutable std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<GameRoom>> rooms;
    std::mt19937_64 rng;

    std::string generateCodeLocked();

public:
    RoomManager();

    // Upper-cases and strips spaces/dashes, so "abc 123" and "ABC123" match.
    static std::string normalizeCode(const std::string& raw);

    // Creates a brand-new room with a unique code. Returns nullptr (and sets
    // error) if the server is at its room limit.
    std::shared_ptr<GameRoom> createRoom(std::string& error);

    // Returns nullptr if no such room exists. Never creates one.
    std::shared_ptr<GameRoom> findRoom(const std::string& code) const;

    // Deletes the room if (and only if) nobody is connected to it.
    void removeIfEmpty(const std::shared_ptr<GameRoom>& room);

    std::size_t roomCount() const;
};

#endif
