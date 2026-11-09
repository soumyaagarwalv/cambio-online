
#include "Card.h"

Card::Card(Rank rank, Color color)
    : rank(rank), color(color) {}

Rank Card::getRank() const {
    return rank;
}

Color Card::getColor() const {
    return color;
}

int Card::getPoints() const {
    switch (rank) {
        case Rank::ACE:
            return 1;

        case Rank::TWO:
            return 2;

        case Rank::THREE:
            return 3;

        case Rank::FOUR:
            return 4;

        case Rank::FIVE:
            return 5;

        case Rank::SIX:
            return 6;

        case Rank::SEVEN:
            return 7;

        case Rank::EIGHT:
            return 8;

        case Rank::NINE:
            return 9;

        case Rank::TEN:
            return 10;

        case Rank::JACK:
            return 11;

        case Rank::QUEEN:
            return 12;

        case Rank::KING:
            return color == Color::RED ? -1 : 13;

        case Rank::JOKER:
            return 0;
    }

    return 0;
}

CardPower Card::getPower() const {
    switch (rank) {
        case Rank::SEVEN:
        case Rank::EIGHT:
            return CardPower::PEEK_OWN;

        case Rank::NINE:
        case Rank::TEN:
            return CardPower::PEEK_OPPONENT;

        case Rank::JACK:
        case Rank::QUEEN:
            return CardPower::BLIND_SWAP;

        case Rank::KING:
            return CardPower::LOOK_AND_SWAP;

        default:
            return CardPower::NONE;
    }
}

std::string Card::toString() const {
    // Point value as text: A=1, J=11, Q=12, red K=-1, black K=13, Joker=0.
    // The face letter is available separately through getLabel().
    return std::to_string(getPoints());
}

std::string Card::getLabel() const {
    switch (rank) {
        case Rank::ACE: return "A";
        case Rank::TWO: return "2";
        case Rank::THREE: return "3";
        case Rank::FOUR: return "4";
        case Rank::FIVE: return "5";
        case Rank::SIX: return "6";
        case Rank::SEVEN: return "7";
        case Rank::EIGHT: return "8";
        case Rank::NINE: return "9";
        case Rank::TEN: return "10";
        case Rank::JACK: return "J";
        case Rank::QUEEN: return "Q";
        case Rank::KING: return "K";
        case Rank::JOKER: return "JKR";
    }
    return "?";
}
