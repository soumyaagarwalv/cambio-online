
#include "Player.h"

#include <stdexcept>
#include <utility>

Player::Player(
    const std::string& name,
    int playerIndex,
    int totalPlayers,
    const std::array<Card, CARD_COUNT>& initialCards,
    const std::array<int, 2>& initialPeekPositions
)
    : name(name),
      playerIndex(playerIndex),
      knowledge(totalPlayers) {

    if (playerIndex < 0 || playerIndex >= totalPlayers) {
        throw std::invalid_argument("Invalid player index");
    }

    for (int position = 0; position < CARD_COUNT; position++) {
        cards[position] = initialCards[position];
    }

    for (int position : initialPeekPositions) {
        if (position < 0 || position >= CARD_COUNT) {
            throw std::out_of_range("Invalid initial peek position");
        }

        knowledge[playerIndex][position] = cards[position];
    }
}

std::string Player::getName() const {
    return name;
}

int Player::getPlayerIndex() const {
    return playerIndex;
}

Card Player::getCard(int position) const {
    if (position < 0 || position >= CARD_COUNT) {
        throw std::out_of_range("Invalid card position");
    }

    if (!cards[position].has_value()) {
        throw std::runtime_error("There is no card at this position");
    }

    return *cards[position];
}

bool Player::hasCardAtPosition(int position) const {
    if (position < 0 || position >= CARD_COUNT) {
        throw std::out_of_range("Invalid card position");
    }

    return cards[position].has_value();
}

std::optional<Card> Player::getKnownCard(
    int targetPlayer,
    int position
) const {
    if (position < 0 || position >= CARD_COUNT) {
        throw std::out_of_range("Invalid card position");
    }

    return knowledge.at(targetPlayer).at(position);
}

void Player::observeCard(
    int targetPlayer,
    int position,
    const Card& card
) {
    if (position < 0 || position >= CARD_COUNT) {
        throw std::out_of_range("Invalid card position");
    }

    knowledge.at(targetPlayer).at(position) = card;
}

void Player::forgetCard(int targetPlayer, int position) {
    if (position < 0 || position >= CARD_COUNT) {
        throw std::out_of_range("Invalid card position");
    }

    knowledge.at(targetPlayer).at(position).reset();
}

void Player::clearAllKnowledge() {
    for (auto& observerKnowledge : knowledge) {
        for (auto& positionKnowledge : observerKnowledge) {
            positionKnowledge.reset();
        }
    }
}

Card Player::removeCardAtPosition(int position) {
    if (position < 0 || position >= CARD_COUNT) {
        throw std::out_of_range("Invalid card position");
    }

    if (!cards[position].has_value()) {
        throw std::runtime_error("There is no card at this position");
    }

    Card removed = *cards[position];
    cards[position].reset();

    return removed;
}

Card Player::removeCardForAllObservers(
    std::vector<Player>& players,
    int targetPlayer,
    int position
) {
    Player& target = players.at(targetPlayer);

    // Remove the actual card from the fixed grid.
    Card removed = target.removeCardAtPosition(position);

    // Every observer must forget the card at this position.
    for (Player& observer : players) {
        observer.forgetCard(targetPlayer, position);
    }

    return removed;
}

void Player::addExtraCard(const Card& card) {
    extraCards.push_back(card);
}

Card Player::removeExtraCard(int extraIndex) {
    if (extraIndex < 0 ||
        extraIndex >= static_cast<int>(extraCards.size())) {
        throw std::out_of_range("Invalid extra-card index");
    }

    Card removed = extraCards[extraIndex];
    extraCards.erase(extraCards.begin() + extraIndex);
    return removed;
}

Card Player::getExtraCard(int extraIndex) const {
    if (extraIndex < 0 || extraIndex >= static_cast<int>(extraCards.size())) {
        throw std::out_of_range("Invalid extra-card index");
    }
    return extraCards[extraIndex];
}

int Player::getExtraCardCount() const {
    return static_cast<int>(extraCards.size());
}

int Player::getCardCount() const {
    int count = static_cast<int>(extraCards.size());

    for (const auto& card : cards) {
        if (card.has_value()) {
            count++;
        }
    }

    return count;
}

void Player::swapCardsBetweenPlayers(
    std::vector<Player>& players,
    int playerA,
    int positionA,
    int playerB,
    int positionB
) {
    if (positionA < 0 || positionA >= CARD_COUNT ||
        positionB < 0 || positionB >= CARD_COUNT) {
        throw std::out_of_range("Invalid swap position");
    }

    Player& first = players.at(playerA);
    Player& second = players.at(playerB);

    if (!first.cards[positionA].has_value() ||
        !second.cards[positionB].has_value()) {
        throw std::runtime_error("Cannot swap an empty position");
    }

    std::swap(
        first.cards[positionA],
        second.cards[positionB]
    );

    // Knowledge follows the publicly observed movement of cards.
    for (Player& observer : players) {
        std::swap(
            observer.knowledge.at(playerA).at(positionA),
            observer.knowledge.at(playerB).at(positionB)
        );
    }
}

void Player::replaceCardForAllObservers(
    std::vector<Player>& players,
    int targetPlayer,
    int position,
    const Card& newCard
) {
    if (position < 0 || position >= CARD_COUNT) {
        throw std::out_of_range("Invalid card position");
    }

    players.at(targetPlayer).cards[position] = newCard;

    // The new identity is unknown to other observers unless
    // they explicitly learn it through another game action.
    for (Player& observer : players) {
        observer.knowledge.at(targetPlayer).at(position).reset();
    }
}

int Player::calculateScore() const {
    int total = 0;

    for (const auto& card : cards) {
        if (card.has_value()) {
            total += card->getPoints();
        }
    }

    for (const Card& card : extraCards) {
        total += card.getPoints();
    }

    return total;
}
