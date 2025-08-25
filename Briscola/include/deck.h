#pragma once
#include <vector>
#include "card.h"

class Deck {
private:
    std::vector<Card> cards;
    Card briscola;

public:
    Deck();
    void shuffle();
    Card draw();
    bool empty() const;
    Card getBriscola() const;
};
