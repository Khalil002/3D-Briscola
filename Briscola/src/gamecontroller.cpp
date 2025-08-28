#include "gamecontroller.h"
#include <iostream>
#include <cstdlib>
#include <ctime>

std::vector<Card> GameController::getDeck(){
    return deck.getCards();
}

int GameController::getCpuHandSize(){
    return cpu.hand.size();
}

int GameController::getPlayerHandSize(){
    return player.hand.size();
}

bool GameController::IsPlayerTurn(){
    return isPlayerTurn;
}

int cardStrength(const Card& card) {
    switch (card.value) {
        case 1:  return 10; // Ace strongest
        case 3:  return 9;  // Then 3
        case 10: return 8;  // King (Re)
        case 9:  return 7;  // Knight (Cavallo)
        case 8:  return 6;  // Jack (Fante)
        case 7:  return 5;
        case 6:  return 4;
        case 5:  return 3;
        case 4:  return 2;
        case 2:  return 1;
        default: return 0;  // should never happen
    }
}

void GameController::resetGame(){
    deck = Deck();
    player = Player();
    cpu = Player();
    briscola = Card();
    run();
}

void GameController::run() {
    std::srand(std::time(nullptr));
    deck.shuffle();
    briscola = deck.getBriscola();
    isPlayerTurn = true;

    //dealInitialCards();

    std::cout << "Welcome to Briscola!\n";
    std::cout << "Briscola suit: " << briscola.toString() << "\n\n";

    /**while (player.HasCards() || !deck.empty()) {
        playTurn();
    }

    displayFinalResult();**/
}

void GameController::dealInitialCards() {
    for (int i = 0; i < 3; ++i) {
        player.DrawFromDeck(deck);
        cpu.DrawFromDeck(deck);
    }
    player.ShowHand();
    cpu.ShowHand();
}

bool GameController::playTurn(int choice, int cpuChoice) {
    /**std::cout << "\n--- New Round ---\n";
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

    //Card playerCard = player.PlayCard(choice - 1);**/
    Card playerCard = player.PlayCard(choice);
    //int cpuChoice = std::rand() % cpu.hand.size();
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

    //drawCards(isPlayerTurn);

    std::cout << "Score — You: " << player.points << " | CPU: " << cpu.points << "\n";
    return playerWins;
}

void GameController::drawCards(bool isPlayerTurn){
    if (isPlayerTurn) {
        player.DrawFromDeck(deck);
        cpu.DrawFromDeck(deck);
    } else {
        cpu.DrawFromDeck(deck);
        player.DrawFromDeck(deck);
    }
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

bool GameController::beats(const Card& first, const Card& second, Suit briscolaSuit, Suit ledSuit) {
    // If second card is briscola and first is not -> second wins
    if (second.suit == briscolaSuit && first.suit != briscolaSuit) {
        return true;
    }
    // If first is briscola and second is not -> first wins
    if (first.suit == briscolaSuit && second.suit != briscolaSuit) {
        return false;
    }

    // If both are same suit (could both be briscola too)
    if (second.suit == first.suit) {
        return cardStrength(second) > cardStrength(first);
    }

    // If second didn’t follow the lead suit -> it loses
    if (second.suit != ledSuit) {
        return false;
    }

    // Otherwise, second followed the led suit
    return cardStrength(second) > cardStrength(first);
}


