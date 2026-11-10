#include "server/RoomManager.h"

#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Client {
    std::vector<std::string> inbox;
    GameRoom::SendFunction sender() {
        return [this](const std::string& m) { inbox.push_back(m); };
    }
    bool saw(const std::string& needle) const {
        for (const auto& m : inbox) if (m.find(needle) != std::string::npos) return true;
        return false;
    }
    std::string lastWith(const std::string& needle) const {
        for (auto it = inbox.rbegin(); it != inbox.rend(); ++it)
            if (it->find(needle) != std::string::npos) return *it;
        return {};
    }
};
}

int main() {
    std::string err;

    // --- codes: unique, well-formed, never auto-created by lookup -----------
    {
        RoomManager rm;
        std::set<std::string> codes;
        std::vector<std::shared_ptr<GameRoom>> keep;
        for (int i = 0; i < 200; ++i) {
            auto r = rm.createRoom(err);
            assert(r);
            assert(r->id().size() == RoomManager::kCodeLength);
            assert(codes.insert(r->id()).second);
            keep.push_back(r);
        }
        assert(rm.roomCount() == 200);
        assert(rm.findRoom("NOPE42") == nullptr);
        assert(rm.roomCount() == 200);                 // lookup did not create
        const std::string code = keep[0]->id();
        std::string lower = code;
        for (auto& c : lower) c = static_cast<char>(std::tolower(c));
        assert(rm.findRoom(lower) == keep[0]);          // case-insensitive
        assert(rm.findRoom(" " + code.substr(0, 3) + "-" + code.substr(3)) == keep[0]);
    }

    // --- two rooms are fully independent ------------------------------------
    {
        RoomManager rm;
        auto a = rm.createRoom(err);
        auto b = rm.createRoom(err);
        assert(a != b && a->id() != b->id());

        Client a0, a1, b0, b1;
        int id = -1;
        assert(a->join("Alice", a0.sender(), id, err) && id == 0);
        assert(a->join("Bob", a1.sender(), id, err) && id == 1);
        assert(b->join("Rahul", b0.sender(), id, err) && id == 0);
        assert(b->join("Neha", b1.sender(), id, err) && id == 0 + 1);

        assert(!a0.saw("Rahul") && !a1.saw("Neha"));    // no cross-talk in lobby

        assert(a->start(0, err));
        assert(a->isStarted() && !b->isStarted());      // starting A leaves B alone
        assert(!b0.saw("\"type\":\"state\""));

        // Moves in room A reach only room A.
        const auto b0Before = b0.inbox.size();
        Action ready{ActionType::CONFIRM_READY};
        assert(a->handleAction(0, ready, err));
        assert(b0.inbox.size() == b0Before);

        assert(b->start(0, err));
        assert(b->isStarted());
    }

    // --- only the host starts; need two players -----------------------------
    {
        RoomManager rm;
        auto r = rm.createRoom(err);
        Client c0, c1;
        int id0, id1;
        assert(r->join("Host", c0.sender(), id0, err));
        assert(!r->start(id0, err));                    // alone
        assert(r->join("Guest", c1.sender(), id1, err));
        assert(!r->start(id1, err));                    // guest cannot start
        assert(c0.lastWith("\"type\":\"lobby\"").find("\"host\":true") != std::string::npos);
        assert(c1.lastWith("\"type\":\"lobby\"").find("\"host\":false") != std::string::npos);
        assert(r->start(id0, err));
        int late;
        assert(!r->join("Late", Client().sender(), late, err));   // closed after start
    }

    // --- leaving the lobby: host handover and gap-free seats ----------------
    {
        RoomManager rm;
        auto r = rm.createRoom(err);
        Client c0, c1, c2;
        int i0, i1, i2;
        assert(r->join("A", c0.sender(), i0, err));
        assert(r->join("B", c1.sender(), i1, err));
        assert(r->join("C", c2.sender(), i2, err));
        r->disconnect(i0);                               // host leaves
        assert(r->playerCount() == 2);
        assert(!r->start(i2, err));                      // C is not host, B is
        assert(c1.lastWith("\"type\":\"lobby\"").find("\"host\":true") != std::string::npos);
        assert(r->start(i1, err));
        assert(c1.saw("\"type\":\"seat\",\"playerId\":0"));
        assert(c2.saw("\"type\":\"seat\",\"playerId\":1"));
        // Seat 1 (connection id 2) can act using its connection id.
        Action ready{ActionType::CONFIRM_READY};
        assert(r->handleAction(i2, ready, err));
    }

    // --- full room, and cleanup ---------------------------------------------
    {
        RoomManager rm;
        auto r = rm.createRoom(err);
        std::vector<Client> cs(5);
        int id;
        std::vector<int> ids;
        for (int i = 0; i < 4; ++i) { assert(r->join("P", cs[i].sender(), id, err)); ids.push_back(id); }
        assert(!r->join("P5", cs[4].sender(), id, err));
        const std::string code = r->id();

        for (int i = 0; i < 3; ++i) { r->disconnect(ids[i]); rm.removeIfEmpty(r); }
        assert(rm.findRoom(code));                       // someone still inside
        r->disconnect(ids[3]);
        rm.removeIfEmpty(r);
        assert(rm.findRoom(code) == nullptr);            // last one out
        assert(rm.roomCount() == 0);
        assert(!r->join("Ghost", cs[4].sender(), id, err));  // closed room rejects joiners
    }

    // --- names are sanitised -------------------------------------------------
    {
        RoomManager rm;
        auto r = rm.createRoom(err);
        Client c;
        int id;
        assert(r->join("  Bob \"the\" \\ Builder with a very long name indeed  ", c.sender(), id, err));
        const std::string lobby = c.lastWith("\"type\":\"lobby\"");
        assert(lobby.find("\\\"the\\\"") != std::string::npos);   // quotes escaped
        assert(lobby.find("indeed") == std::string::npos);        // truncated
        Client d;
        assert(r->join("", d.sender(), id, err));
        assert(d.lastWith("\"type\":\"lobby\"").find("Player 2") != std::string::npos);
    }

    // --- concurrency: many threads creating/joining/leaving -----------------
    {
        RoomManager rm;
        std::vector<std::thread> ts;
        for (int t = 0; t < 16; ++t) {
            ts.emplace_back([&rm]() {
                for (int i = 0; i < 50; ++i) {
                    std::string e;
                    auto r = rm.createRoom(e);
                    Client a, b;
                    int ia, ib;
                    r->join("a", a.sender(), ia, e);
                    if (auto same = rm.findRoom(r->id())) same->join("b", b.sender(), ib, e);
                    r->disconnect(ia);
                    rm.removeIfEmpty(r);
                    r->disconnect(ib);
                    rm.removeIfEmpty(r);
                }
            });
        }
        for (auto& t : ts) t.join();
        assert(rm.roomCount() == 0);
    }

    std::cout << "RoomManager tests passed\n";
    return 0;
}
