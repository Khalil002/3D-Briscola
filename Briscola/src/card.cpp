#include "card.h"

/**
 * @brief Converts the card object to a string representation.
 *
 * This method generates a string that represents the card in the format:
 * "<value> di <suit>". For example, "Asso di Coppe" or "10 di Spade".
 *
 * @return A string representation of the card.
 */
std::string Card::toString() const {
    std::string name;

    // Determine the name of the card based on its value.
    switch (value) {
        case 1: name = "Asso"; break;  // Ace
        case 8: name = "Fante"; break; // Knave
        case 9: name = "Cavallo"; break; // Knight
        case 10: name = "Re"; break; // King
        default: name = std::to_string(value); break; // Numeric values
    }

    std::string nameSuit;

    // Determine the name of the suit.
    switch (suit) {
        case Suit::COPPE: nameSuit = "Coppe"; break; // Cups
        case Suit::DENARI: nameSuit = "Denari"; break; // Coins
        case Suit::SPADE: nameSuit = "Spade"; break; // Swords
        case Suit::BASTONI: nameSuit = "Bastoni"; break; // Clubs
    }

    // Combine the value and suit into the final string representation.
    return name + " di " + nameSuit;
}
