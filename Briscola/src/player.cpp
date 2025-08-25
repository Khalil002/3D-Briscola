#include "player.h"
#include <iostream>
#include <vector>

void Player::DrawFromDeck(Deck& deck) {
    if (!deck.empty()) {
         hand.push_back(deck.draw());
    }
}

void Player::ShowHand() const {
    std::cout << "Your cards:\n";
    for (size_t i = 0; i < hand.size(); ++i) {
        std::cout << i + 1 << ") " << hand[i].toString() << "\n";
    }
}

Card Player::PlayCard(int index) {
    if (index < 0 || index >= static_cast<int>(hand.size())) {
        throw std::runtime_error("Index not valid");
    }
    Card choice = hand[index];
    hand.erase(hand.begin() + index);
    return choice;
}

bool Player::HasCards() const {
    return !hand.empty();
}

