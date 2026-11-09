#include "server/Protocol.h"
#include "server/RoomManager.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;
namespace fs = std::filesystem;
using tcp = asio::ip::tcp;

// ---------------------------------------------------------------------------
// Static file serving, so ONE port serves both the web page and the game
// WebSocket. This is what makes a single tunnel / hosted deployment work.
// ---------------------------------------------------------------------------

static std::string mimeType(const fs::path& path) {
    const std::string ext = path.extension().string();
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".png")  return "image/png";
    if (ext == ".ico")  return "image/x-icon";
    return "application/octet-stream";
}

static fs::path findWebRoot(const std::string& requested) {
    std::vector<fs::path> candidates;
    if (!requested.empty()) candidates.push_back(requested);
    if (const char* env = std::getenv("CAMBIO_WEB_DIR")) candidates.push_back(env);
    candidates.push_back("web");
    candidates.push_back("../web");
    candidates.push_back("/app/web");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c / "index.html", ec)) return fs::canonical(c, ec);
    }
    return {};
}

static void serveHttp(tcp::socket& socket,
                      const http::request<http::string_body>& req,
                      const fs::path& webRoot) {
    auto reply = [&](http::status status, const std::string& type,
                     const std::string& body) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, "cambio");
        res.set(http::field::content_type, type);
        res.set(http::field::cache_control, "no-cache");
        res.keep_alive(false);
        res.body() = body;
        res.prepare_payload();
        beast::error_code ec;
        http::write(socket, res, ec);
        socket.shutdown(tcp::socket::shutdown_send, ec);
    };

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        reply(http::status::method_not_allowed, "text/plain", "Method not allowed");
        return;
    }

    std::string target = std::string(req.target());
    const auto cut = target.find_first_of("?#");
    if (cut != std::string::npos) target.erase(cut);

    if (target == "/healthz") {
        reply(http::status::ok, "text/plain", "ok");
        return;
    }
    if (target.empty() || target == "/") target = "/index.html";

    if (webRoot.empty() || target.find("..") != std::string::npos ||
        target.find('\\') != std::string::npos) {
        reply(http::status::not_found, "text/plain", "Not found");
        return;
    }

    const fs::path file = webRoot / target.substr(1);
    std::ifstream in(file, std::ios::binary);
    if (!in || fs::is_directory(file)) {
        reply(http::status::not_found, "text/plain", "Not found");
        return;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    reply(http::status::ok, mimeType(file),
          req.method() == http::verb::head ? std::string() : buf.str());
}

class ClientSession : public std::enable_shared_from_this<ClientSession> {
private:
    websocket::stream<tcp::socket> ws;
    RoomManager& rooms;
    std::mutex writeMutex;
    std::shared_ptr<GameRoom> room;
    int playerId = -1;

public:
    ClientSession(tcp::socket socket, RoomManager& manager)
        : ws(std::move(socket)), rooms(manager) {}

    void run(const http::request<http::string_body>& upgradeRequest) {
        beast::error_code ec;
        ws.accept(upgradeRequest, ec);
        if (ec) return;

        for (;;) {
            beast::flat_buffer buffer;
            ws.read(buffer, ec);
            if (ec) break;

            const std::string message = beast::buffers_to_string(buffer.data());
            handle(message);
        }

        if (room && playerId >= 0) room->disconnect(playerId);
    }

    void send(const std::string& message) {
        std::lock_guard<std::mutex> lock(writeMutex);
        beast::error_code ec;
        ws.text(true);
        ws.write(asio::buffer(message), ec);
    }

private:
    void handle(const std::string& raw) {
        std::string error;
        const auto command = parseClientCommand(raw, error);
        if (!command) {
            send(makeErrorMessage(error));
            return;
        }

        switch (command->type) {
            case ClientCommand::Type::JOIN:
                handleJoin(*command);
                break;
            case ClientCommand::Type::START:
                handleStart();
                break;
            case ClientCommand::Type::ACTION:
                handleAction(command->action);
                break;
            case ClientCommand::Type::PING:
                send("{\"type\":\"pong\"}");
                break;
            default:
                send(makeErrorMessage("Unsupported command."));
                break;
        }
    }

    void handleJoin(const ClientCommand& command) {
        if (room) {
            send(makeErrorMessage("You already joined a room."));
            return;
        }

        room = rooms.getOrCreate(command.roomId.empty() ? "default" : command.roomId);

        const auto weakSelf = weak_from_this();
        std::string error;
        int assignedId = -1;
        if (!room->join(command.playerName,
                        [weakSelf](const std::string& message) {
                            if (auto self = weakSelf.lock()) self->send(message);
                        },
                        assignedId,
                        error)) {
            room.reset();
            send(makeErrorMessage(error));
            return;
        }

        playerId = assignedId;
        send(makeInfoMessage("Joined successfully. Wait for the host to start the game."));
    }

    void handleStart() {
        if (!room || playerId < 0) {
            send(makeErrorMessage("Join a room first."));
            return;
        }

        if (playerId != 0) {
            send(makeErrorMessage("Only the host can start the game."));
            return;
        }

        std::string error;
        if (!room->start(error)) {
            send(makeErrorMessage(error));
            return;
        }
    }

    void handleAction(const Action& action) {
        if (!room || playerId < 0) {
            send(makeErrorMessage("Join a room first."));
            return;
        }

        std::string error;
        if (!room->handleAction(playerId, action, error)) {
            send(makeErrorMessage(error));
        }
    }
};

int main(int argc, char** argv) {
    // Port: first argument, else the PORT environment variable (used by
    // hosting platforms), else 8080. Optional second argument: web folder.
    unsigned short port = 8080;
    if (argc > 1) {
        port = static_cast<unsigned short>(std::stoi(argv[1]));
    } else if (const char* envPort = std::getenv("PORT")) {
        port = static_cast<unsigned short>(std::stoi(envPort));
    }
    const fs::path webRoot = findWebRoot(argc > 2 ? argv[2] : "");

    try {
        asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), port));
        RoomManager rooms;

        std::cout << "Cambio server listening on port " << port << "\n";
        if (webRoot.empty()) {
            std::cout << "Web folder not found: serving the game WebSocket only.\n";
        } else {
            std::cout << "Serving web page from " << webRoot.string()
                      << "  ->  open http://localhost:" << port << "\n";
        }

        for (;;) {
            tcp::socket socket(io);
            acceptor.accept(socket);

            std::thread(
                [socket = std::move(socket), &rooms, webRoot]() mutable {
                    beast::flat_buffer buffer;
                    http::request<http::string_body> req;
                    beast::error_code ec;
                    http::read(socket, buffer, req, ec);
                    if (ec) return;

                    if (websocket::is_upgrade(req)) {
                        std::make_shared<ClientSession>(
                            std::move(socket), rooms
                        )->run(req);
                    } else {
                        serveHttp(socket, req, webRoot);
                    }
                }
            ).detach();
        }
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << '\n';
        return 1;
    }
}
