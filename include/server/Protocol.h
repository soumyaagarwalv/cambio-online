#ifndef CAMBIO_SERVER_PROTOCOL_H
#define CAMBIO_SERVER_PROTOCOL_H

#include "Action.h"
#include <string>
#include <optional>

struct ClientCommand {
    enum class Type { JOIN, START, ACTION, PING, UNKNOWN };
    Type type = Type::UNKNOWN;
    std::string roomId;
    std::string playerName;
    Action action{};
};

std::optional<ClientCommand> parseClientCommand(const std::string& json,
                                                std::string& error);
std::string makeErrorMessage(const std::string& message);
std::string makeInfoMessage(const std::string& message);

#endif
