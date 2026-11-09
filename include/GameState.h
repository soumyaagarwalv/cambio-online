#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "Deck.h"
#include "Player.h"

#include <string>
#include <vector>

enum class GamePhase {
    MEMORIZATION,
    PLAYING,
    FINAL_ROUND,
    FINISHED
};

class GameState {
private:
    Deck deck;
    std::vector<Player> players;

    struct DiscardEntry {
        Card card;
        int ownerPlayerIndex;
        bool successfulFlipUsed;
    };

    // The last entry is the top discard. Metadata stays with each discard
    // so drawing the top card reveals the correct owner/status underneath.
    std::vector<DiscardEntry> discardPile;

    int currentPlayerIndex;
    GamePhase phase;

public:
    explicit GameState(
        const std::vector<std::string>& playerNames
    );

    Deck& getDeck();
    const Deck& getDeck() const;

    std::vector<Player>& getPlayers();
    const std::vector<Player>& getPlayers() const;

    void addToDiscardPile(const Card& card, int ownerPlayerIndex = -1);
    Card takeTopDiscard();
    Card getTopDiscard() const;
    int getTopDiscardOwner() const;
    bool hasSuccessfulFlipOnTopDiscard() const;
    void markSuccessfulFlipOnTopDiscard();
    bool isDiscardPileEmpty() const;

    int getCurrentPlayerIndex() const;
    void setCurrentPlayerIndex(int index);
    void advanceTurn();

    GamePhase getPhase() const;
    void setPhase(GamePhase newPhase);

    int getPlayerCount() const;
};

#endif