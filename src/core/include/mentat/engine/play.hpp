// SPDX-License-Identifier: GPL-3.0-or-later
//
// The game itself, from the moment the story ends: the palace, the control
// panel and what the player does with them. See docs/rules/game.md.
#pragma once

namespace mentat {

class Game;

// Starts a new game in the palace and plays it. Returns when the player
// leaves the game.
void runGame(Game& game, int language);

} // namespace mentat
