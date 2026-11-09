
#ifndef CARD_H
#define CARD_H

#include <string>

enum class Rank {
    ACE,
    TWO,
    THREE,
    FOUR,
    FIVE,
    SIX,
    SEVEN,
    EIGHT,
    NINE,
    TEN,
    JACK,
    QUEEN,
    KING,
    JOKER
};

enum class Color {
    RED,
    BLACK,
    NONE
};

enum class CardPower {
    NONE,
    PEEK_OWN,
    PEEK_OPPONENT,
    BLIND_SWAP,
    LOOK_AND_SWAP
};

class Card {
private:
    Rank rank;
    Color color;

public:
    Card(Rank rank, Color color);

    Rank getRank() const;
    Color getColor() const;

    int getPoints() const;
    CardPower getPower() const;

    std::string toString() const;

    // Face label shown in the top-left of a card: A, 2-10, J, Q, K, JOKER.
    std::string getLabel() const;
};

#endif
