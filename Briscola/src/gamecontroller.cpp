#include "gamecontroller.h"
#include <iostream>
#include <cstdlib>
#include <ctime>

void GameController::run() {
    std::srand(std::time(nullptr));
    deck.shuffle();
    briscola = deck.getBriscola();
    isPlayerTurn = true;

    dealInitialCards();

    std::cout << "Welcome to Briscola!\n";
    std::cout << "Briscola suit: " << briscola.toString() << "\n\n";

    while (player.HasCards() || !deck.empty()) {
        playTurn();
    }

    displayFinalResult();
}

void GameController::dealInitialCards() {
    for (int i = 0; i < 3; ++i) {
        player.DrawFromDeck(deck);
        cpu.DrawFromDeck(deck);
    }
}

void GameController::playTurn() {
    std::cout << "\n--- New Round ---\n";
    player.ShowHand();

    int choice;
    std::cout << "Choose a card to play (1-" << player.hand.size() << "): ";
    std::cin >> choice;
    while (std::cin.fail() || choice < 1 || choice > static_cast<int>(player.hand.size())) {
        std::cin.clear();
        std::cin.ignore(1000, '\n');
        std::cout << "Invalid choice. Try again: ";
        std::cin >> choice;
    }

    Card playerCard = player.PlayCard(choice - 1);
    int cpuChoice = std::rand() % cpu.hand.size();
    Card cpuCard = cpu.PlayCard(cpuChoice);

    std::cout << "You played: " << playerCard.toString() << "\n";
    std::cout << "CPU played: " << cpuCard.toString() << "\n";

    Card first, second;
    bool playerWins = false;

    if (isPlayerTurn) {
        first = playerCard;
        second = cpuCard;
        playerWins = !beats(first, second, briscola.suit, first.suit);
    } else {
        first = cpuCard;
        second = playerCard;
        playerWins = beats(first, second, briscola.suit, first.suit);
    }

    int points = playerCard.points + cpuCard.points;

    if (playerWins) {
        player.points += points;
        std::cout << "You won the hand! +" << points << " points\n";
        isPlayerTurn = true;
    } else {
        cpu.points += points;
        std::cout << "CPU won the hand. +" << points << " points\n";
        isPlayerTurn = false;
    }

    if (!deck.empty()) {
        if (isPlayerTurn) {
            player.DrawFromDeck(deck);
            cpu.DrawFromDeck(deck);
        } else {
            cpu.DrawFromDeck(deck);
            player.DrawFromDeck(deck);
        }
    }

    std::cout << "Score — You: " << player.points << " | CPU: " << cpu.points << "\n";
}

void GameController::displayFinalResult() {
    std::cout << "\n--- Game Over ---\n";
    std::cout << "Your points: " << player.points << "\n";
    std::cout << "CPU points: " << cpu.points << "\n";

    if (player.points > cpu.points) {
        std::cout << "Congratulations! You won!\n";
    } else if (cpu.points > player.points) {
        std::cout << "The CPU won!\n";
    } else {
        std::cout << "It's a tie!\n";
    }
}

bool GameController::beats(const Card& first, const Card& second, Suit briscolaSuit, Suit playedSuit) {
    if (second.suit == first.suit) {
        return second.value > first.value;
    } else if (second.suit == briscolaSuit && first.suit != briscolaSuit) {
        return true;
    } else {
        return false;
    }
}
