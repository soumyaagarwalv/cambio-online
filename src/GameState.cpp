#include "GameState.h"

#include <array>
#include <random>
#include <stdexcept>

GameState::GameState(
    const std::vector<std::string>& playerNames
)
    : currentPlayerIndex(0),
      phase(GamePhase::MEMORIZATION) {

    if (playerNames.size() < 2) {
        throw std::invalid_argument(
            "A game requires at least two players"
        );
    }

    // Four grid cards per player. The discard pile starts empty;
    // the first discard is created naturally when a player resolves a draw.
    if (playerNames.size() * 4 >
        static_cast<std::size_t>(deck.size())) {
        throw std::invalid_argument(
            "Not enough cards for the number of players"
        );
    }

    for (std::size_t i = 0; i < playerNames.size(); ++i) {
        const std::array<Card, 4> hand = {
            deck.draw(),
            deck.draw(),
            deck.draw(),
            deck.draw()
        };

        // Each player initially knows positions 0 and 1.
        const std::array<int, 2> initialPeekPositions = {0, 1};

        players.emplace_back(
            playerNames[i],
            static_cast<int>(i),
            static_cast<int>(playerNames.size()),
            hand,
            initialPeekPositions
        );
    }

}

Deck& GameState::getDeck() {
    return deck;
}

const Deck& GameState::getDeck() const {
    return deck;
}

std::vector<Player>& GameState::getPlayers() {
    return players;
}

const std::vector<Player>& GameState::getPlayers() const {
    return players;
}

void GameState::addToDiscardPile(
    const Card& card,
    int ownerPlayerIndex
) {
    if (ownerPlayerIndex < -1 ||
        ownerPlayerIndex >= static_cast<int>(players.size())) {
        throw std::out_of_range("Invalid discard owner");
    }

    discardPile.push_back({card, ownerPlayerIndex, false});
}

Card GameState::getTopDiscard() const {
    if (discardPile.empty()) {
        throw std::runtime_error("The discard pile is empty");
    }

    return discardPile.back().card;
}

int GameState::getTopDiscardOwner() const {
    if (discardPile.empty()) {
        throw std::runtime_error("The discard pile is empty");
    }

    return discardPile.back().ownerPlayerIndex;
}

bool GameState::hasSuccessfulFlipOnTopDiscard() const {
    if (discardPile.empty()) {
        throw std::runtime_error("The discard pile is empty");
    }

    return discardPile.back().successfulFlipUsed;
}

void GameState::markSuccessfulFlipOnTopDiscard() {
    if (discardPile.empty()) {
        throw std::runtime_error("The discard pile is empty");
    }

    discardPile.back().successfulFlipUsed = true;
}

bool GameState::isDiscardPileEmpty() const {
    return discardPile.empty();
}

int GameState::getCurrentPlayerIndex() const {
    return currentPlayerIndex;
}

void GameState::setCurrentPlayerIndex(int index) {
    if (index < 0 ||
        index >= static_cast<int>(players.size())) {

        throw std::out_of_range(
            "Invalid current player index"
        );
    }

    currentPlayerIndex = index;
}

void GameState::advanceTurn() {
    currentPlayerIndex =
        (currentPlayerIndex + 1) %
        static_cast<int>(players.size());
}

GamePhase GameState::getPhase() const {
    return phase;
}

void GameState::setPhase(GamePhase newPhase) {
    phase = newPhase;
}

int GameState::getPlayerCount() const {
    return static_cast<int>(players.size());
}

Card GameState::takeTopDiscard() {
    if (discardPile.empty()) {
        throw std::runtime_error(
            "Cannot draw from an empty discard pile"
        );
    }

    Card topCard = discardPile.back().card;
    discardPile.pop_back();

    return topCard;
}