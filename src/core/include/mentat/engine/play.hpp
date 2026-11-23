// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game itself, from the moment the story ends: the palace, the control
// panel and what the player does with them. See docs/rules/game.md.
#pragma once

namespace mentat {

class Game;

enum class GameEnd {
    Left, // the player left the game
    Won,  // the story came to its end and the cast was shown: the original then starts over
};

// Starts a new game in the palace and plays it. Returns when the player
// leaves the game or the story ends.
GameEnd runGame(Game& game, int language);

} // namespace mentat
