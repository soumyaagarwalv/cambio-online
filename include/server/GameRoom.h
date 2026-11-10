#ifndef CAMBIO_SERVER_GAME_ROOM_H
#define CAMBIO_SERVER_GAME_ROOM_H

#include "GameEngine.h"
#include "GameState.h"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// One independent game. Owns its own GameState + GameEngine, so nothing a
// player does here can touch any other room.
//
// Two kinds of ids are used:
//   * connection id ("seat" before the game starts): stable, assigned on join.
//     This is what the network layer holds on to.
//   * player index: the position inside GameState/GameEngine. It is fixed when
//     the game starts (players are ordered by connection id), so players who
//     leave the lobby before the start never leave gaps.
class GameRoom {
public:
    using SendFunction = std::function<void(const std::string&)>;
    static constexpr int kMaxPlayers = 4;

private:
    struct Connection {
        std::string name;
        SendFunction send;
    };

    mutable std::mutex mutex;
    std::string roomId;
    std::map<int, Connection> connections;       // connection id -> connection
    std::unordered_map<int, int> engineIndexOf;  // connection id -> player index
    std::unique_ptr<GameState> state;
    std::unique_ptr<GameEngine> engine;
    int nextConnectionId = 0;
    bool closed = false;

    void broadcastViewsLocked();
    void broadcastLobbyLocked();
    void sendViewLocked(int connectionId);
    int hostIdLocked() const;

public:
    explicit GameRoom(std::string id);

    bool join(const std::string& name,
              SendFunction send,
              int& connectionId,
              std::string& error);

    // Only the host (lowest connection id still present) may start.
    bool start(int connectionId, std::string& error);

    bool handleAction(int connectionId,
                      const Action& action,
                      std::string& error);

    void disconnect(int connectionId);

    // Marks the room closed if nobody is connected. After this returns true,
    // join() always fails, so a late joiner can never land in a deleted room.
    bool closeIfEmpty();

    bool isStarted() const;
    int playerCount() const;
    std::string id() const;
};

#endif
