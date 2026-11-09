#ifndef CAMBIO_SERVER_GAME_ROOM_H
#define CAMBIO_SERVER_GAME_ROOM_H

#include "GameEngine.h"
#include "GameState.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class GameRoom {
public:
    using SendFunction = std::function<void(const std::string&)>;

private:
    struct Connection {
        std::string name;
        SendFunction send;
    };

    mutable std::mutex mutex;
    std::string roomId;
    std::vector<std::string> names;
    std::unordered_map<int, Connection> connections;
    std::unique_ptr<GameState> state;
    std::unique_ptr<GameEngine> engine;
    int nextPlayerId = 0;

    void broadcastViewsLocked();
    void broadcastLobbyLocked();
    void sendViewLocked(int playerId);

public:
    explicit GameRoom(std::string id);

    bool join(const std::string& name,
              SendFunction send,
              int& playerId,
              std::string& error);

    bool start(std::string& error);

    bool handleAction(int playerId,
                      const Action& action,
                      std::string& error);

    void disconnect(int playerId);

    bool isStarted() const;
    int playerCount() const;
    std::string id() const;
};

#endif
