
# 3D-Briscola
A 3D <a href="https://en.wikipedia.org/wiki/Briscola">Briscola</a> game made in Vulkan

## Overview

Briscola is an italian card games, it is played using an italian 40-card deck and it can be played in many different configurations. In this project, we implement the classic briscola 1v1 game, where you, the player, plays a briscola game against the CPU in a 3D enviroment.

![](https://github.com/user-attachments/assets/b1aa46a6-2107-4157-9e00-eeaa7f9f7ed8)


## Controls
Menu Controls
- UP / DOWN arrow keys → Move the menu selection (toggle between "PLAY" and "EXIT").
- SPACE → Confirm menu selection
  
Gameplay Controls
- LEFT / RIGHT arrow keys → Cycle through your hand of cards.
- SPACE → Play the currently highlighted card.
- Number keys 1, 2, 3 → Directly play the corresponding card in your hand (1 = leftmost, 3 = rightmost). Useful for quick selection.
- R → Restart the game (only works if the game is over).
- ESC → Exit the game.
- 9 → Toggle camera snap mode (camera locks/unlocks).

## Usage
To run the project you must:
1. Install Vulkan in your system
2. Clone the project
3. Build the cmake
4. Run Briscola
