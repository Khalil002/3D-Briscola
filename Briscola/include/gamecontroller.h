#ifndef GAME_CONTROLLER_H
#define GAME_CONTROLLER_H

#include "player.h"
#include "deck.h"
#include "card.h"
#include <thread>

class GameController {
public:
    void run();
    //void start();
    //void stop();
    void dealInitialCards();
    bool playTurn(int choice, int cpuChoice);
    void displayFinalResult();
    bool beats(const Card& first, const Card& second, Suit briscolaSuit, Suit playedSuit);
    std::vector<Card> getDeck();
    int getCpuHandSize();
    int getPlayerHandSize();
    bool IsPlayerTurn();
    void drawCards(bool isPlayerTurn);
private:
    //~GameController();
    Deck deck;
    Player player;
    Player cpu;
    Card briscola;
    bool isPlayerTurn;
    std::atomic<bool> stopRequested{false};
    std::thread worker;
};

#endif

