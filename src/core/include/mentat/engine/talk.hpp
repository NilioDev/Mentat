// SPDX-License-Identifier: GPL-3.0-or-later
//
// Talking with a character: the face over an enlarged part of the room,
// the menu of what Paul can say, and the lines the character answers with,
// shown as subtitles or spoken. See docs/rules/talking.md.
#pragma once

#include "mentat/data/phrase_file.hpp"
#include "mentat/engine/dialogue.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/gfx/scene.hpp"
#include "mentat/gfx/surface.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mentat {

class Game;
class Panel;
class World;

// What talking needs from the rest of the game.
class TalkHost {
public:
    virtual ~TalkHost() = default;
    // The ornament at the top of the panel, closed step by step or opened.
    virtual void closeOrnament() = 0;
    // Keeps what the back buffer shows behind the ornament, for closing it.
    virtual void keepOrnamentBackground() = 0;
    virtual void openOrnament() = 0;
    // Draws the ornament as it is on the back buffer: the original puts it
    // over the scene each time it shows the back buffer.
    virtual void drawOrnament() = 0;
    virtual void showPointer(bool shown) = 0;
    // The list of commands was replaced.
    virtual void menuChanged() = 0;
    // Talking is over: the room comes back.
    virtual void backToRoom() = 0;
};

class Talk {
public:
    // How the lines reach the player, as the original's mixer panel offers.
    enum class Subtitles {
        Below = 0,    // the text at the bottom of the picture, no voice
        Balloons = 1, // the voice and the text (shown below for now)
        None = 2,     // the voice only
    };

    // The original's numbers for the talk menu's commands.
    static constexpr std::uint16_t kTalkToMe = 0x9472;
    static constexpr std::uint16_t kComeWithMe = 0x95E2;
    static constexpr std::uint16_t kStopFollowing = 0x9533;
    static constexpr std::uint16_t kWhat = 0x9ED5;
    static constexpr std::uint16_t kStopTalking = 0xD2E2;
    // Clicking a character: the actions from kFirstCharacter on, five apart,
    // talk to characters 0 to 11.
    static constexpr std::uint16_t kFirstCharacter = 0x92F2;
    static constexpr int kTalkActionStep = 5;
    static constexpr int kTalkActions = 12;
    // The character a person action talks to, or -1.
    static int characterOf(std::uint16_t action);

    Talk(Game& game, World& world, Panel& panel, TalkHost& host, int language);

    bool active() const { return character_ >= 0; }
    // The phrase of the line said last.
    int phrase() const { return phrase_ & 0x3FF; }
    // What the subtitle shows now, if anything.
    bool subtitleShown() const { return subtitle_.shown; }
    int character() const { return character_; }

    Subtitles subtitles = Subtitles::None;

    // Starts talking to character `c`, who says the first line there is.
    // `x` and `y` are where the room showed the character, or negative
    // when it did not.
    void start(int c, int x, int y);
    // A command of the talk menu. False when it is not one.
    bool command(std::uint16_t action);

    // A companion speaks up on the way, over the scene already on the back
    // buffer: the face, then the first line of the story's list of
    // remarks that may be said. False when there is none.
    bool remark(int c);
    // The flight goes on: the remark is over.
    void endRemark();
    bool remarking() const { return remark_; }

    // The story's own list of lines: run when the story moves on, they say
    // nothing but change the game.
    void runEvents();

    // The text that would be shown for a phrase, all its pages, with what
    // the game fills in put in. Exposed for the tests.
    std::vector<std::string> phraseText(int phrase, bool secondFile);

    Dialogue& dialogue() { return dialogue_; }

private:
    struct Subtitle {
        Surface pixels{320, 72};
        int rows = 0; // drawn rows from the top of `pixels`
        int top = 0;  // where they go on the screen
        bool shown = false;
    };

    void prepareCondition();
    bool condition(int n);
    void next();
    void sayLine(std::size_t at, bool events);
    void showPage(const std::string& page);
    void afterLine(std::size_t at);
    void runAction(const DialogueLine& line);
    void raiseProgress(int to);
    void speak();
    void drawBackground(int x, int y);
    void restartFace();
    void drawSubtitle(const ClipRect* clip);
    void showScene();
    void layoutSubtitle(const std::string& text);
    void setMenu();
    void comeWithMe();
    void stopFollowing();
    void leaves(std::size_t record);
    void takeSlot(int number);
    void stop();
    void chooseTake(std::uint8_t code, int named);
    void chooseTakeForNumber(std::uint8_t at, unsigned value);
    void expand(const std::string& in, std::string& out, std::vector<std::string>& pages, int depth, bool secondFile);
    const PhraseFile& phrases(bool second);
    std::uint16_t record() const;

    Game& game_;
    World& world_;
    Panel& panel_;
    TalkHost& host_;
    int language_;
    Dialogue dialogue_;
    Conditions conditions_;
    bool remark_ = false;
    std::array<std::optional<PhraseFile>, 2> phrases_;
    std::optional<Font> large_;
    std::optional<Font> small_;

    int character_ = -1;
    int topic_ = 0;
    std::optional<std::size_t> resume_; // where to look on; nothing: the topic's list
    bool finished_ = false;             // the last line ended the talk
    std::uint8_t skipSaid_ = DialogueLine::kSaid;
    std::uint8_t answer_ = 0;  // a line's yes (0xFF) or no
    bool endAfter_ = false;
    std::vector<std::string> pages_; // the pages of the line still to show
    std::size_t pageLine_ = 0;
    std::uint8_t lineFlags_ = 0; // the flags of the line being said
    int take_ = 0;               // which recording of it
    int phrase_ = 0;      // the last phrase, with 0x1000 for each page after the first
    bool secondFile_ = false;
    int words_ = 0;       // in the last text laid out
    Subtitles mode_ = Subtitles::None; // for the line being said
    Subtitle subtitle_;
    std::vector<std::uint16_t> notes_; // the lines the original notes down
};

} // namespace mentat
