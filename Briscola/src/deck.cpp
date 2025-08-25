#include "deck.h"
#include <algorithm>
#include <random>
#include <ctime>
#include <stdexcept>

Deck::Deck() {
    for (int s = 0; s < 4; ++s) {
        for (int v = 1; v <= 10; ++v) {
            int points = 0;
            if (v == 1) points = 11;       // Asso/Ace
            else if (v == 3) points = 10; // Tre/Three
            else if (v == 10) points = 4;  // Re/King
            else if (v == 9) points = 3;   // Cavallo/Knight
            else if (v == 8) points = 2;   // Fante/Knave
            else points = 0;               // Other cards

            Card c = { static_cast<Suit>(s), v, points };
            cards.push_back(c);
        }
    }
}

void Deck::shuffle() {
    //std::srand(unsigned(std::time(nullptr)));
    //std::random_shuffle(cards.begin(), cards.end());
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(cards.begin(), cards.end(), g);

    // Ultima carta è la briscola (non rimossa dal mazzo)
    briscola = cards.back();
}

Card Deck::draw() {
    if (cards.empty()) {
        throw std::runtime_error("Empty deck");
    }
    Card c = cards.front();
    cards.erase(cards.begin());
    return c;
}

bool Deck::empty() const {
    return cards.empty();
}

Card Deck::getBriscola() const {
    return briscola;


}
