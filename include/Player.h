
#ifndef PLAYER_H
#define PLAYER_H

#include "Card.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

class Player {
private:
    static constexpr int CARD_COUNT = 4;

    std::string name;
    int playerIndex;

    // The four grid positions never move.
    // A successful flip can leave a position empty.
    std::array<std::optional<Card>, CARD_COUNT> cards;

    // Additional cards held outside the fixed 2x2 grid.
    std::vector<Card> extraCards;

    // Private knowledge for this observer:
    // knowledge[targetPlayer][position]
    std::vector<std::array<std::optional<Card>, CARD_COUNT>>
        knowledge;

public:
    Player(
        const std::string& name,
        int playerIndex,
        int totalPlayers,
        const std::array<Card, CARD_COUNT>& initialCards,
        const std::array<int, 2>& initialPeekPositions
    );

    std::string getName() const;
    int getPlayerIndex() const;

    // Throws if the position is invalid or empty.
    Card getCard(int position) const;

    bool hasCardAtPosition(int position) const;

    std::optional<Card> getKnownCard(
        int targetPlayer,
        int position
    ) const;

    void observeCard(
        int targetPlayer,
        int position,
        const Card& card
    );

    void forgetCard(int targetPlayer, int position);

    // At the end of the initial memorization phase, all remembered
    // identities are cleared from the authoritative knowledge model.
    // Players are expected to remember their two initially revealed cards.
    void clearAllKnowledge();

    // Removes a card from a fixed grid position.
    Card removeCardAtPosition(int position);

    static Card removeCardForAllObservers(
    std::vector<Player>& players,
    int targetPlayer,
    int position
);

    // Additional cards are kept outside the 2x2 grid.
    void addExtraCard(const Card& card);
    Card removeExtraCard(int extraIndex);
    Card getExtraCard(int extraIndex) const;
    int getExtraCardCount() const;

    // Total cards held, including extra cards.
    int getCardCount() const;

    static void swapCardsBetweenPlayers(
        std::vector<Player>& players,
        int playerA,
        int positionA,
        int playerB,
        int positionB
    );

    static void replaceCardForAllObservers(
        std::vector<Player>& players,
        int targetPlayer,
        int position,
        const Card& newCard
    );

    int calculateScore() const;
};

#endif
