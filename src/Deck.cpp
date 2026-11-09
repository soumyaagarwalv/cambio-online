
#include "Deck.h"

#include <algorithm>
#include <stdexcept>
#include <random>

Deck::Deck()
    : rng(std::random_device{}()) {
    initialize();
    shuffle();
}

void Deck::initialize() {
    cards.clear();

    // Add the 4 suits' worth of cards.
    // We only track RED/BLACK because suits have no game mechanics.
    for (int i = 0; i < 4; i++) {

        Color color = (i < 2) ? Color::RED : Color::BLACK;

        // Add A through 10
        for (int value = 0; value <= 9; value++) {
            Rank rank = static_cast<Rank>(value);

            cards.emplace_back(rank, color);
        }

        // Add Jack, Queen, King
        cards.emplace_back(Rank::JACK, color);
        cards.emplace_back(Rank::QUEEN, color);
        cards.emplace_back(Rank::KING, color);
    }

    // Add 2 Jokers
    cards.emplace_back(Rank::JOKER, Color::NONE);
    cards.emplace_back(Rank::JOKER, Color::NONE);
}

void Deck::shuffle() {
    std::shuffle(cards.begin(), cards.end(), rng);
}

Card Deck::draw() {
    if (cards.empty()) {
        throw std::runtime_error("Cannot draw from an empty deck");
    }

    Card topCard = cards.back();
    cards.pop_back();

    return topCard;
}

int Deck::size() const {
    return static_cast<int>(cards.size());
}

bool Deck::isEmpty() const {
    return cards.empty();
}
