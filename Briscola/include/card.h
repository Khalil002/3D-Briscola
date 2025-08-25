#pragma once
#include <string>

enum class Suit { COPPE, DENARI, SPADE, BASTONI };

// Briscola points:
// A=11, 3=10, Re=4, Cavallo=3, Fante=2, other=0

struct Card {
    Suit suit;
    int value;      // from ace (1) to 10
    int points;   // based on value (ex. Ace = 11)

    std::string toString() const;
};

