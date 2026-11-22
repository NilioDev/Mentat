// SPDX-License-Identifier: GPL-3.0-or-later
//
// The opening: the logos, Irulan's introduction, the title and the story
// up to the start of the game, played to the beat of the music the way the
// original's opening script does. See docs/rules/intro.md.
#pragma once

namespace mentat {

class Game;

// The original's language numbers, as its command line options set them.
inline constexpr int kFrench = 1;
inline constexpr int kGerman = 2;
inline constexpr int kEnglish = 3;
inline constexpr int kItalian = 4;
inline constexpr int kSpanish = 5;

enum class IntroEnd {
    Finished, // every step played: the original then starts it over
    Skipped,  // the player pressed a key or a button
};

IntroEnd runIntro(Game& game, int language);

// After the opening is skipped: the credits video, which a key ends too.
void runCredits(Game& game);

enum class StoryEnd {
    Finished, // every page shown, and the palace at dawn
    Skipped,  // the player pressed a key or a button
};

// The pages that tell the story, each a picture with words under it, and
// the palace at dawn. See docs/rules/story.md.
StoryEnd runStory(Game& game, int language);

} // namespace mentat
