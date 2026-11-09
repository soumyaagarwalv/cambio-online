#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "Action.h"
#include "GameEvent.h"
#include "GameState.h"

#include <optional>
#include <string>
#include <vector>

class GameEngine {
private:
    GameState& state;
    int cambioCaller = -1;
    std::vector<bool> finalTurnTaken;
    std::vector<int> winnerPlayers;
    bool endedByPenalty = false;
    int penaltyLoser = -1;
    std::optional<Card> pendingDraw;
    int pendingDrawPlayer = -1;
    bool drawnFromDiscard = false;

    // A correct opponent flip is a two-step interaction: first the target
    // card is flipped/removed, then the flipper chooses a card to give.
    bool pendingFlipGift = false;
    int pendingFlipPlayer = -1;
    int pendingFlipTarget = -1;
    // King power, step 1 done: the two looked-at cards wait for a swap decision.
    bool pendingLook = false;
    int pendingLookPlayer = -1;
    int pendingLookCards[4] = {-1, -1, -1, -1};  // playerA, posA, playerB, posB
    std::vector<bool> ready;
    std::vector<GameEvent> events;

    void finishTurn(int playerIndex);
    void finishGameAfterCambio();
    void finishGameAfterPenalty(int loser);
    bool finishDrawAction(int playerIndex, std::string& error);

public:
    explicit GameEngine(GameState& gameState);

    bool performAction(int playerIndex, const Action& action, std::string& error);

    bool isReady(int playerIndex) const;
    int getPenaltyLoser() const;
    int getCambioCaller() const;
    const std::vector<int>& getWinnerPlayers() const;
    std::optional<Card> getPendingDrawForPlayer(int playerIndex) const;
    bool hasPendingDrawForPlayer(int playerIndex) const;
    bool isPendingDrawFromDiscard() const;
    CardPower getPendingDrawPower() const;
    bool hasPendingLookForPlayer(int playerIndex) const;
    const int* getPendingLookCards() const;   // {playerA, posA, playerB, posB}
    bool hasPendingFlipGiftForPlayer(int playerIndex) const;
    int getPendingFlipGiftTarget() const;
    const std::vector<GameEvent>& getEvents() const;
    void clearEvents();
};

#endif
