#ifndef GAME_CONTROLLER_H
#define GAME_CONTROLLER_H

#include "player.h"
#include "deck.h"
#include "card.h"

class GameController {
public:
    void run();

private:
    Deck deck;
    Player player;
    Player cpu;
    Card briscola;
    bool isPlayerTurn;

    void dealInitialCards();
    void playTurn();
    void displayFinalResult();
    bool beats(const Card& first, const Card& second, Suit briscolaSuit, Suit playedSuit);
};

#endif

