
#ifndef DECK_H
#define DECK_H

#include "Card.h"
#include <vector>
#include <random>

class Deck {
private:
    std::vector<Card> cards;
    std::mt19937 rng;

public:
    Deck();

    void initialize();
    void shuffle();

    Card draw();

    int size() const;
    bool isEmpty() const;
};

#endif
