
#include "GameEngine.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

GameEngine::GameEngine(GameState& gameState)
    : state(gameState),
      finalTurnTaken(gameState.getPlayerCount(), false),
      ready(gameState.getPlayerCount(), false) {
}

void GameEngine::finishGameAfterCambio() {
    const auto& players = state.getPlayers();
    const int callerScore = players.at(cambioCaller).calculateScore();

    bool callerStrictlyLowest = true;
    for (int i = 0; i < static_cast<int>(players.size()); ++i) {
        if (i != cambioCaller &&
            callerScore >= players[i].calculateScore()) {
            callerStrictlyLowest = false;
            break;
        }
    }

    winnerPlayers.clear();
    if (callerStrictlyLowest) {
        winnerPlayers.push_back(cambioCaller);
    } else {
        // The caller loses on a tie. All non-callers tied for the lowest
        // score are recorded as winners.
        int lowestNonCallerScore = 0;
        bool hasLowest = false;
        for (int i = 0; i < static_cast<int>(players.size()); ++i) {
            if (i == cambioCaller) {
                continue;
            }
            const int score = players[i].calculateScore();
            if (!hasLowest || score < lowestNonCallerScore) {
                lowestNonCallerScore = score;
                hasLowest = true;
            }
        }

        for (int i = 0; i < static_cast<int>(players.size()); ++i) {
            if (i != cambioCaller &&
                players[i].calculateScore() == lowestNonCallerScore) {
                winnerPlayers.push_back(i);
            }
        }
    }

    state.setPhase(GamePhase::FINISHED);

    GameEvent finishedEvent{};
    finishedEvent.type = GameEventType::GAME_FINISHED;
    finishedEvent.player = cambioCaller;
    finishedEvent.winnerPlayers = winnerPlayers;
    events.push_back(finishedEvent);
}

void GameEngine::finishGameAfterPenalty(int loser) {
    endedByPenalty = true;
    penaltyLoser = loser;
    winnerPlayers.clear();

    const auto& players = state.getPlayers();
    int lowestScore = 0;
    bool hasLowest = false;

    for (int i = 0; i < static_cast<int>(players.size()); ++i) {
        if (i == loser) {
            continue;
        }
        const int score = players[i].calculateScore();
        if (!hasLowest || score < lowestScore) {
            lowestScore = score;
            hasLowest = true;
        }
    }

    for (int i = 0; i < static_cast<int>(players.size()); ++i) {
        if (i != loser && players[i].calculateScore() == lowestScore) {
            winnerPlayers.push_back(i);
        }
    }

    state.setPhase(GamePhase::FINISHED);

    GameEvent eliminatedEvent{};
    eliminatedEvent.type = GameEventType::PLAYER_ELIMINATED;
    eliminatedEvent.player = loser;
    events.push_back(eliminatedEvent);

    GameEvent finishedEvent{};
    finishedEvent.type = GameEventType::GAME_FINISHED;
    finishedEvent.player = loser;
    finishedEvent.winnerPlayers = winnerPlayers;
    events.push_back(finishedEvent);
}

void GameEngine::finishTurn(int playerIndex) {
    if (state.getPhase() == GamePhase::FINAL_ROUND) {
        finalTurnTaken.at(playerIndex) = true;

        bool everyoneFinished = true;
        for (int i = 0; i < state.getPlayerCount(); ++i) {
            if (i != cambioCaller && !finalTurnTaken[i]) {
                everyoneFinished = false;
                break;
            }
        }

        if (everyoneFinished) {
            finishGameAfterCambio();
            return;
        }
    }

    state.advanceTurn();

    // The caller's turn is already over. Skip them if the index wraps around.
    if (state.getPhase() == GamePhase::FINAL_ROUND &&
        state.getCurrentPlayerIndex() == cambioCaller) {
        state.advanceTurn();
    }
}

bool GameEngine::finishDrawAction(
    int playerIndex,
    std::string& error
) {
    if (!pendingDraw.has_value() ||
        pendingDrawPlayer != playerIndex) {
        error = "There is no pending draw for this player.";
        return false;
    }

    pendingDraw.reset();
    pendingDrawPlayer = -1;
    drawnFromDiscard = false;

    finishTurn(playerIndex);
    return true;
}

bool GameEngine::performAction(
    int playerIndex,
    const Action& action,
    std::string& error
) {
    error.clear();

    if (playerIndex < 0 ||
        playerIndex >= state.getPlayerCount()) {
        error = "Invalid player index.";
        return false;
    }

    // 1. Initial readiness
    if (state.getPhase() == GamePhase::MEMORIZATION) {
        if (action.type != ActionType::CONFIRM_READY) {
            error = "Confirm readiness before the game starts.";
            return false;
        }

        if (ready[playerIndex]) {
            error = "This player has already confirmed.";
            return false;
        }

        ready[playerIndex] = true;

        GameEvent readyEvent{};
        readyEvent.type = GameEventType::PLAYER_READY;
        readyEvent.player = playerIndex;
        events.push_back(readyEvent);

        bool everyoneReady = true;

        for (bool playerReady : ready) {
            if (!playerReady) {
                everyoneReady = false;
                break;
            }
        }

        if (everyoneReady) {
            // The initial two cards were shown only for memorization.
            // Once everyone confirms, clear the server-side remembered
            // identities so the normal game begins with all four cards
            // face-down again. Later peeks explicitly recreate knowledge.
            for (Player& player : state.getPlayers()) {
                player.clearAllKnowledge();
            }

            state.setPhase(GamePhase::PLAYING);

            GameEvent startEvent{};
            startEvent.type = GameEventType::GAME_STARTED;
            events.push_back(startEvent);
        }

        return true;
    }

    // A correct opponent flip requires the flipper to choose a card to give.
    // This is deliberately a separate action so the UI can simply let the
    // player click the opponent card first, then choose the gift.
    if (action.type == ActionType::GIVE_CARD_AFTER_FLIP) {
        if (!pendingFlipGift || pendingFlipPlayer != playerIndex) {
            error = "There is no pending card to give after a flip.";
            return false;
        }

        auto& flipPlayers = state.getPlayers();
        auto validPosition = [](int position) {
            return position >= 0 && position < 4;
        };

        std::optional<Card> givenCard;
        GameEvent giveEvent{};
        giveEvent.type = GameEventType::FLIP_SUCCEEDED;
        giveEvent.player = playerIndex;
        giveEvent.targetPlayer = pendingFlipTarget;
        giveEvent.position = action.position;
        giveEvent.targetPosition = -1;

        if (action.extraCardIndex >= 0) {
            if (action.extraCardIndex >=
                flipPlayers[playerIndex].getExtraCardCount()) {
                error = "Invalid extra-card index to give.";
                return false;
            }
            givenCard = flipPlayers[playerIndex].removeExtraCard(
                action.extraCardIndex
            );
            giveEvent.position = -1;
            giveEvent.transferredExtraCard = true;
        } else {
            if (!validPosition(action.position) ||
                !flipPlayers[playerIndex].hasCardAtPosition(action.position)) {
                error = "Choose one of your cards to give.";
                return false;
            }

            givenCard = Player::removeCardForAllObservers(
                flipPlayers, playerIndex, action.position
            );
        }

        flipPlayers[pendingFlipTarget].addExtraCard(*givenCard);

        pendingFlipGift = false;
        pendingFlipPlayer = -1;
        const int target = pendingFlipTarget;
        pendingFlipTarget = -1;

        events.push_back(giveEvent);

        if (flipPlayers[target].getCardCount() > 6) {
            finishGameAfterPenalty(target);
        }

        return true;
    }

    // FLIP_CARD is permitted outside the normal turn order.
    if (action.type == ActionType::FLIP_CARD) {
        if (state.getPhase() != GamePhase::PLAYING &&
            state.getPhase() != GamePhase::FINAL_ROUND) {
            error = "Flipping is not allowed in this phase.";
            return false;
        }

        if (cambioCaller == playerIndex) {
            error = "The player who called Cambio cannot flip.";
            return false;
        }

        if (state.isDiscardPileEmpty()) {
            error = "There is no discard card to match.";
            return false;
        }

        if (state.hasSuccessfulFlipOnTopDiscard()) {
            error = "A successful flip has already occurred for this discard.";
            return false;
        }

        if (state.getTopDiscardOwner() == playerIndex) {
            error = "You cannot flip the discard you just made.";
            return false;
        }

        auto& flipPlayers = state.getPlayers();
        auto validFlipPosition = [](int position) {
            return position >= 0 && position < 4;
        };
        auto validFlipPlayer = [&](int index) {
            return index >= 0 && index < state.getPlayerCount();
        };

        // targetPlayer == -1 means flip your own card at action.position.
        // Otherwise targetPlayer/targetPosition identify an opponent's card,
        // and action.position identifies the card you give them on success.
        const bool ownFlip = (action.targetPlayer == -1);
        const int flippedPlayer = ownFlip ? playerIndex : action.targetPlayer;
        const int flippedPosition = ownFlip ? action.position : action.targetPosition;

        if (!validFlipPlayer(flippedPlayer) ||
            !validFlipPosition(flippedPosition)) {
            error = "Invalid card location to flip.";
            return false;
        }

        if (cambioCaller != -1 && flippedPlayer == cambioCaller) {
            error = "The Cambio caller's cards are protected.";
            return false;
        }

        if (!flipPlayers[flippedPlayer].hasCardAtPosition(flippedPosition)) {
            error = "There is no card at that grid position.";
            return false;
        }

        if (!ownFlip && flippedPlayer == playerIndex) {
            error = "Use targetPlayer = -1 to flip your own card.";
            return false;
        }

        if (!ownFlip && flipPlayers[playerIndex].getCardCount() == 0) {
            error = "You need a card to give after a successful opponent flip.";
            return false;
        }

        if (action.extraCardIndex < -1) {
            error = "Invalid extra-card index.";
            return false;
        }

        const Card flippedCard = flipPlayers[flippedPlayer].getCard(flippedPosition);
        const Card topDiscard = state.getTopDiscard();
        const bool matched = flippedCard.getRank() == topDiscard.getRank();

        if (matched) {
            GameEvent flipEvent{};
            flipEvent.type = GameEventType::FLIP_SUCCEEDED;
            flipEvent.player = playerIndex;
            flipEvent.targetPlayer = flippedPlayer;
            flipEvent.targetPosition = flippedPosition;
            flipEvent.publicCard = flippedCard;

            // A correct flip always sends the matched card to the discard
            // pile (owned by the flipper, so it cannot be flipped again for
            // this discard). The slot it leaves behind is empty.
            Player::removeCardForAllObservers(
                flipPlayers, flippedPlayer, flippedPosition
            );
            state.addToDiscardPile(flippedCard, playerIndex);
            state.markSuccessfulFlipOnTopDiscard();

            if (!ownFlip) {
                // Opponent flip: the flipper must now give one of their own
                // cards to the opponent whose card was matched.
                pendingFlipGift = true;
                pendingFlipPlayer = playerIndex;
                pendingFlipTarget = flippedPlayer;
            }

            events.push_back(flipEvent);
            return true;
        }

        // Wrong flip: take a penalty card into the extra-card area.
        if (state.getDeck().isEmpty()) {
            error = "Wrong flip, but the deck is empty; no penalty card is available.";
            return false;
        }

        const Card penaltyCard = state.getDeck().draw();
        flipPlayers[playerIndex].addExtraCard(penaltyCard);

        GameEvent failedEvent{};
        failedEvent.type = GameEventType::FLIP_FAILED;
        failedEvent.player = playerIndex;
        failedEvent.targetPlayer = flippedPlayer;
        failedEvent.targetPosition = flippedPosition;
        failedEvent.publicCard = flippedCard;
        events.push_back(failedEvent);

        if (flipPlayers[playerIndex].getCardCount() > 6) {
            finishGameAfterPenalty(playerIndex);
        }

        return true;
    }

    if (state.getPhase() != GamePhase::PLAYING &&
        state.getPhase() != GamePhase::FINAL_ROUND) {
        error = "The game is not accepting normal actions.";
        return false;
    }

    if (playerIndex != state.getCurrentPlayerIndex()) {
        error = "It is not this player's turn.";
        return false;
    }

    // King power, step 2: after looking at two cards the player decides.
    if (pendingLook) {
        if (action.type != ActionType::DECIDE_SWAP) {
            error = "Decide whether to swap the two cards you looked at.";
            return false;
        }
        auto& lp = state.getPlayers();
        const int pa = pendingLookCards[0], xa = pendingLookCards[1];
        const int pb = pendingLookCards[2], xb = pendingLookCards[3];
        // A flip may have removed one of the cards in the meantime; then there is nothing to swap.
        const bool stillThere = lp[pa].hasCardAtPosition(xa) && lp[pb].hasCardAtPosition(xb);
        if (action.shouldSwap && stillThere) {
            Player::swapCardsBetweenPlayers(lp, pa, xa, pb, xb);
            GameEvent swapEvent{};
            swapEvent.type = GameEventType::CARDS_SWAPPED;
            swapEvent.player = playerIndex;          // who used the power
            swapEvent.targetPlayer = pa;             // first card
            swapEvent.targetPosition = xa;
            swapEvent.secondTargetPlayer = pb;       // second card
            swapEvent.secondTargetPosition = xb;
            swapEvent.swapped = true;
            events.push_back(swapEvent);
        }
        pendingLook = false;
        pendingLookPlayer = -1;
        return finishDrawAction(playerIndex, error);
    }
    if (action.type == ActionType::DECIDE_SWAP) {
        error = "There is no swap decision to make.";
        return false;
    }

    // Calling Cambio must happen before drawing. It immediately ends the
    // caller's turn and gives each other player one final turn.
    if (action.type == ActionType::CALL_CAMBIO) {
        if (state.getPhase() != GamePhase::PLAYING) {
            error = "Cambio can only be called during the normal playing phase.";
            return false;
        }

        if (pendingDraw.has_value()) {
            error = "Cambio must be called before drawing a card.";
            return false;
        }

        cambioCaller = playerIndex;
        std::fill(finalTurnTaken.begin(), finalTurnTaken.end(), false);
        finalTurnTaken[playerIndex] = true;
        state.setPhase(GamePhase::FINAL_ROUND);

        GameEvent callEvent{};
        callEvent.type = GameEventType::PLAYER_CALLED_CAMBIO;
        callEvent.player = playerIndex;
        events.push_back(callEvent);

        // The caller's turn ends immediately; the next player begins the
        // final round. The caller is skipped when turn order wraps around.
        state.advanceTurn();
        if (state.getCurrentPlayerIndex() == cambioCaller) {
            state.advanceTurn();
        }
        return true;
    }

    // 2. Draw a card
    if (action.type == ActionType::DRAW_FROM_DECK ||
        action.type == ActionType::DRAW_FROM_DISCARD) {

        if (pendingDraw.has_value()) {
            error = "Resolve your current drawn card first.";
            return false;
        }

        if (action.type == ActionType::DRAW_FROM_DECK) {
            if (state.getDeck().isEmpty()) {
                error = "The deck is empty.";
                return false;
            }

            pendingDraw = state.getDeck().draw();
            drawnFromDiscard = false;
        } else {
            if (state.isDiscardPileEmpty()) {
                error = "The discard pile is empty.";
                return false;
            }

            pendingDraw = state.takeTopDiscard();
            drawnFromDiscard = true;
        }

        pendingDrawPlayer = playerIndex;

        GameEvent drawEvent{};
        drawEvent.type = GameEventType::CARD_DRAWN;
        drawEvent.player = playerIndex;
        drawEvent.fromDiscardPile = drawnFromDiscard;

        // The discard-pile card is already public.
        // A deck-drawn card remains private.
        if (drawnFromDiscard) {
            drawEvent.publicCard = *pendingDraw;
        }

        events.push_back(drawEvent);
        return true;
    }

    if (!pendingDraw.has_value() ||
        pendingDrawPlayer != playerIndex) {
        error = "You must draw a card first.";
        return false;
    }

    auto& players = state.getPlayers();

    auto validPosition = [](int position) {
        return position >= 0 && position < 4;
    };

    auto validPlayer = [&](int index) {
        return index >= 0 && index < state.getPlayerCount();
    };

    // 3. Replace one of your cards with the drawn card.
    if (action.type == ActionType::REPLACE_CARD_WITH_DRAWN) {
        if (!validPosition(action.position)) {
            error = "Card position must be between 0 and 3.";
            return false;
        }

        Card oldCard = players[playerIndex].getCard(
            action.position
        );

        Card newCard = *pendingDraw;

        Player::replaceCardForAllObservers(
            players,
            playerIndex,
            action.position,
            newCard
        );

        // The acting player saw the drawn card.
        players[playerIndex].observeCard(
            playerIndex,
            action.position,
            newCard
        );

        // The replaced card becomes public.
        state.addToDiscardPile(oldCard, playerIndex);

        GameEvent replaceEvent{};
        replaceEvent.type = GameEventType::CARD_REPLACED;
        replaceEvent.player = playerIndex;
        replaceEvent.position = action.position;
        replaceEvent.publicCard = oldCard;
        events.push_back(replaceEvent);

        return finishDrawAction(playerIndex, error);
    }

    // 4. Discard a non-power card drawn from the deck.
    if (action.type == ActionType::DISCARD_DRAWN_CARD) {
        if (drawnFromDiscard) {
            error =
                "A card drawn from the discard pile must replace one of your cards.";
            return false;
        }

        if (pendingDraw->getPower() != CardPower::NONE) {
            error =
                "This card has a power. Use its power instead.";
            return false;
        }

        Card discarded = *pendingDraw;
        state.addToDiscardPile(discarded, playerIndex);

        GameEvent discardEvent{};
        discardEvent.type = GameEventType::CARD_DISCARDED;
        discardEvent.player = playerIndex;
        discardEvent.publicCard = discarded;
        events.push_back(discardEvent);

        return finishDrawAction(playerIndex, error);
    }

    // 5. Special powers.
    // Powers can only be activated on a deck draw.
    if (action.type == ActionType::PEEK_OWN_CARD ||
        action.type == ActionType::PEEK_OPPONENT_CARD ||
        action.type == ActionType::BLIND_SWAP ||
        action.type == ActionType::LOOK_AND_OPTIONALLY_SWAP ||
        action.type == ActionType::LOOK_TWO_CARDS ||
        action.type == ActionType::USE_POWER_AND_DISCARD_DRAWN) {

        if (drawnFromDiscard) {
            error =
                "A card drawn from the discard pile cannot activate a power.";
            return false;
        }

        CardPower power = pendingDraw->getPower();
        bool validPowerAction = false;

        switch (action.type) {
            case ActionType::PEEK_OWN_CARD:
                validPowerAction =
                    power == CardPower::PEEK_OWN;
                break;

            case ActionType::PEEK_OPPONENT_CARD:
                validPowerAction =
                    power == CardPower::PEEK_OPPONENT;
                break;

            case ActionType::BLIND_SWAP:
                validPowerAction =
                    power == CardPower::BLIND_SWAP;
                break;

            case ActionType::LOOK_AND_OPTIONALLY_SWAP:
            case ActionType::LOOK_TWO_CARDS:
                validPowerAction =
                    power == CardPower::LOOK_AND_SWAP;
                break;

            default:
                validPowerAction = false;
                break;
        }

        if (!validPowerAction) {
            error =
                "This action does not match the drawn card's power.";
            return false;
        }

        // Determine and validate the relevant card locations.
        int firstPlayer = -1;
        int firstPosition = -1;
        int secondPlayer = -1;
        int secondPosition = -1;

        if (action.type == ActionType::PEEK_OWN_CARD) {
            firstPlayer = playerIndex;
            firstPosition = action.position;

            if (!validPosition(firstPosition)) {
                error = "Invalid card position.";
                return false;
            }
        }
        else if (action.type == ActionType::PEEK_OPPONENT_CARD) {
            secondPlayer = action.targetPlayer;
            secondPosition = action.targetPosition;

            if (!validPlayer(secondPlayer) ||
                secondPlayer == playerIndex ||
                !validPosition(secondPosition)) {
                error =
                    "Choose a valid card belonging to another player.";
                return false;
            }
        }
        else {
            // Both swap powers can target any two card locations,
            // including locations belonging to different players.
            firstPlayer = action.targetPlayer;
            firstPosition = action.targetPosition;
            secondPlayer = action.secondTargetPlayer;
            secondPosition = action.secondTargetPosition;

            if (!validPlayer(firstPlayer) ||
                !validPosition(firstPosition) ||
                !validPlayer(secondPlayer) ||
                !validPosition(secondPosition)) {
                error = "Invalid card locations for the power.";
                return false;
            }

            if (firstPlayer == secondPlayer &&
                firstPosition == secondPosition) {
                error = "Choose two different card positions.";
                return false;
            }
        }

        // Empty grid slots can exist after successful flips. Never allow a
        // peek or swap power to read or manipulate an empty slot.
        if (action.type == ActionType::PEEK_OWN_CARD &&
            !players[firstPlayer].hasCardAtPosition(firstPosition)) {
            error = "There is no card at that position.";
            return false;
        }
        if (action.type == ActionType::PEEK_OPPONENT_CARD &&
            !players[secondPlayer].hasCardAtPosition(secondPosition)) {
            error = "There is no card at that position.";
            return false;
        }
        if ((action.type == ActionType::BLIND_SWAP ||
             action.type == ActionType::LOOK_AND_OPTIONALLY_SWAP ||
             action.type == ActionType::LOOK_TWO_CARDS) &&
            (!players[firstPlayer].hasCardAtPosition(firstPosition) ||
             !players[secondPlayer].hasCardAtPosition(secondPosition))) {
            error = "Cannot use a power on an empty grid position.";
            return false;
        }

        // After Cambio is called, no player may peek at or swap the caller's
        // cards. Validate before discarding the power card so a rejected action
        // does not consume the draw.
        if (state.getPhase() == GamePhase::FINAL_ROUND && cambioCaller != -1) {
            if ((action.type == ActionType::PEEK_OPPONENT_CARD &&
                 secondPlayer == cambioCaller) ||
                ((action.type == ActionType::BLIND_SWAP ||
                  action.type == ActionType::LOOK_AND_OPTIONALLY_SWAP ||
                  action.type == ActionType::LOOK_TWO_CARDS) &&
                 (firstPlayer == cambioCaller || secondPlayer == cambioCaller))) {
                error = "The Cambio caller's cards are protected.";
                return false;
            }
        }

        // Discard the power card publicly.
        Card powerCard = *pendingDraw;
        state.addToDiscardPile(powerCard, playerIndex);

        GameEvent powerDiscardEvent{};
        powerDiscardEvent.type = GameEventType::CARD_DISCARDED;
        powerDiscardEvent.player = playerIndex;
        powerDiscardEvent.publicCard = powerCard;
        events.push_back(powerDiscardEvent);

        // 5a. Peek at one of your own cards.
        if (action.type == ActionType::PEEK_OWN_CARD) {
            Card seen = players[firstPlayer].getCard(firstPosition);

            players[playerIndex].observeCard(
                firstPlayer,
                firstPosition,
                seen
            );

            GameEvent peekEvent{};
            peekEvent.type = GameEventType::CARD_PEEKED;
            peekEvent.player = playerIndex;
            peekEvent.targetPlayer = firstPlayer;
            peekEvent.targetPosition = firstPosition;
            events.push_back(peekEvent);
        }

        // 5b. Peek at another player's card.
        else if (action.type == ActionType::PEEK_OPPONENT_CARD) {
            Card seen = players[secondPlayer].getCard(secondPosition);

            players[playerIndex].observeCard(
                secondPlayer,
                secondPosition,
                seen
            );

            GameEvent peekEvent{};
            peekEvent.type = GameEventType::CARD_PEEKED;
            peekEvent.player = playerIndex;
            peekEvent.targetPlayer = secondPlayer;
            peekEvent.targetPosition = secondPosition;
            events.push_back(peekEvent);
        }

        // 5c. Blind swap: identities are not revealed.
        else if (action.type == ActionType::BLIND_SWAP) {
            Player::swapCardsBetweenPlayers(
                players,
                firstPlayer,
                firstPosition,
                secondPlayer,
                secondPosition
            );

            GameEvent swapEvent{};
            swapEvent.type = GameEventType::CARDS_SWAPPED;
            swapEvent.player = playerIndex;          // who used the power
            swapEvent.targetPlayer = firstPlayer;    // first card
            swapEvent.targetPosition = firstPosition;
            swapEvent.secondTargetPlayer = secondPlayer;   // second card
            swapEvent.secondTargetPosition = secondPosition;
            swapEvent.swapped = true;
            events.push_back(swapEvent);
        }

        // 5d'. King, step 1: look at two cards; the swap decision comes next.
        else if (action.type == ActionType::LOOK_TWO_CARDS) {
            players[playerIndex].observeCard(firstPlayer, firstPosition,
                players[firstPlayer].getCard(firstPosition));
            players[playerIndex].observeCard(secondPlayer, secondPosition,
                players[secondPlayer].getCard(secondPosition));

            GameEvent firstPeekEvent{};
            firstPeekEvent.type = GameEventType::CARD_PEEKED;
            firstPeekEvent.player = playerIndex;
            firstPeekEvent.targetPlayer = firstPlayer;
            firstPeekEvent.targetPosition = firstPosition;
            events.push_back(firstPeekEvent);

            GameEvent secondPeekEvent{};
            secondPeekEvent.type = GameEventType::CARD_PEEKED;
            secondPeekEvent.player = playerIndex;
            secondPeekEvent.targetPlayer = secondPlayer;
            secondPeekEvent.targetPosition = secondPosition;
            events.push_back(secondPeekEvent);

            pendingLook = true;
            pendingLookPlayer = playerIndex;
            pendingLookCards[0] = firstPlayer;
            pendingLookCards[1] = firstPosition;
            pendingLookCards[2] = secondPlayer;
            pendingLookCards[3] = secondPosition;
            return true;   // the turn continues until DECIDE_SWAP
        }

        // 5d. Look at two cards, then optionally swap.
        else if (action.type == ActionType::LOOK_AND_OPTIONALLY_SWAP) {
            Card firstCard =
                players[firstPlayer].getCard(firstPosition);

            Card secondCard =
                players[secondPlayer].getCard(secondPosition);

            // Only the acting player receives this knowledge.
            players[playerIndex].observeCard(
                firstPlayer,
                firstPosition,
                firstCard
            );

            players[playerIndex].observeCard(
                secondPlayer,
                secondPosition,
                secondCard
            );

            GameEvent firstPeekEvent{};
            firstPeekEvent.type = GameEventType::CARD_PEEKED;
            firstPeekEvent.player = playerIndex;
            firstPeekEvent.targetPlayer = firstPlayer;
            firstPeekEvent.targetPosition = firstPosition;
            events.push_back(firstPeekEvent);

            GameEvent secondPeekEvent{};
            secondPeekEvent.type = GameEventType::CARD_PEEKED;
            secondPeekEvent.player = playerIndex;
            secondPeekEvent.targetPlayer = secondPlayer;
            secondPeekEvent.targetPosition = secondPosition;
            events.push_back(secondPeekEvent);

            if (action.shouldSwap) {
                Player::swapCardsBetweenPlayers(
                    players,
                    firstPlayer,
                    firstPosition,
                    secondPlayer,
                    secondPosition
                );

                GameEvent swapEvent{};
                swapEvent.type = GameEventType::CARDS_SWAPPED;
                swapEvent.player = playerIndex;          // who used the power
                swapEvent.targetPlayer = firstPlayer;    // first card
                swapEvent.targetPosition = firstPosition;
                swapEvent.secondTargetPlayer = secondPlayer;   // second card
                swapEvent.secondTargetPosition = secondPosition;
                swapEvent.swapped = true;
                events.push_back(swapEvent);
            }
        }

        return finishDrawAction(playerIndex, error);
    }

    error = "Action is not valid at this stage.";
    return false;
}

std::optional<Card> GameEngine::getPendingDrawForPlayer(
    int playerIndex
) const {
    if (!pendingDraw.has_value() ||
        playerIndex != pendingDrawPlayer) {
        return std::nullopt;
    }

    return pendingDraw;
}

bool GameEngine::hasPendingDrawForPlayer(int playerIndex) const {
    return pendingDraw.has_value() && playerIndex == pendingDrawPlayer;
}

bool GameEngine::isPendingDrawFromDiscard() const {
    return pendingDraw.has_value() && drawnFromDiscard;
}

CardPower GameEngine::getPendingDrawPower() const {
    if (!pendingDraw.has_value()) {
        return CardPower::NONE;
    }
    return pendingDraw->getPower();
}

bool GameEngine::hasPendingLookForPlayer(int playerIndex) const {
    return pendingLook && pendingLookPlayer == playerIndex;
}

const int* GameEngine::getPendingLookCards() const {
    return pendingLookCards;
}

bool GameEngine::hasPendingFlipGiftForPlayer(int playerIndex) const {
    return pendingFlipGift && pendingFlipPlayer == playerIndex;
}

int GameEngine::getPendingFlipGiftTarget() const {
    return pendingFlipGift ? pendingFlipTarget : -1;
}

const std::vector<GameEvent>& GameEngine::getEvents() const {
    return events;
}

void GameEngine::clearEvents() {
    events.clear();
}

bool GameEngine::isReady(int playerIndex) const {
    if (playerIndex < 0 || playerIndex >= static_cast<int>(ready.size())) return false;
    return ready[playerIndex];
}

int GameEngine::getPenaltyLoser() const {
    return penaltyLoser;
}

int GameEngine::getCambioCaller() const {
    return cambioCaller;
}

const std::vector<int>& GameEngine::getWinnerPlayers() const {
    return winnerPlayers;
}
