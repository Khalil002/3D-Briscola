#pragma once
#include <vector>
#include "card.h"
#include "deck.h"

class Player {
public:
    std::vector<Card> hand;
    int points = 0;

    void DrawFromDeck(Deck& deck);
    void ShowHand() const;
    Card PlayCard(int index);
    bool HasCards() const;
};
