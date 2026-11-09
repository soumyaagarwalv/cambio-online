#ifndef GAME_EVENT_H
#define GAME_EVENT_H

#include <optional>
#include <vector>
#include "Card.h"

enum class GameEventType {
    PLAYER_READY,
    GAME_STARTED,
    CARD_DRAWN,
    CARD_DISCARDED,
    CARD_REPLACED,
    CARD_PEEKED,
    CARDS_SWAPPED,
    PLAYER_CALLED_CAMBIO,
    FLIP_SUCCEEDED,
    FLIP_FAILED,
    PLAYER_ELIMINATED,
    GAME_FINISHED
};

struct GameEvent {
    GameEventType type;

    int player = -1;
    int targetPlayer = -1;
    int position = -1;
    int targetPosition = -1;
    int secondTargetPlayer = -1;
    int secondTargetPosition = -1;

    std::optional<Card> publicCard;

    bool fromDiscardPile = false;
    bool swapped = false;
    bool transferredExtraCard = false;
    std::vector<int> winnerPlayers;
};

#endif
