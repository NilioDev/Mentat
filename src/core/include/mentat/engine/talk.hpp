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
#include <functional>
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
    // For the Fremen: "WORK FOR ME" (or "RALLY ME"); for the chief of a
    // troop of Paul's, "GIVE ORDERS TO TROOP"; with the Harkonnens'
    // captain held, "OVERPOWER THE PRISONER".
    static constexpr std::uint16_t kWorkForMe = 0x95C1;
    static constexpr std::uint16_t kGiveOrders = 0x5A03;
    static constexpr std::uint16_t kOverpower = 0x9584;
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

    // The Fremen agreed to work for Paul: the caller enlists their troop
    // and returns it; the menu becomes that of its chief.
    std::function<void()> enlisted;
    // "Give orders to troop": the talk ends and the caller opens the
    // orders' screen.
    std::function<void()> ordersAsked;

    // Talking in a box over the map, as a troop's chief does when given
    // orders: the face in a small frame, the text beside it, no voice.
    struct Box {
        ClipRect face;      // where the face shows
        int focusX = 0;     // the point of the portrait at the frame's corner
        int focusY = 0;
        ClipRect text;      // where the text is laid out
        std::uint8_t fill = 0;
        std::uint8_t colour = 0;
    };
    void startBox(int c, int face, int variant, const Box& box);
    // The next line of list `list` of the character's group that may be
    // said, going on from the last one and starting over at the end.
    // False when there is none.
    bool boxLine(int list);
    // The answer to an order: the first line of list `list` that may be
    // said, looked for from its start. The next boxLine() starts its list
    // over.
    bool boxAnswer(int list);
    // Phrase `phrase` of the command file in the box, in its top rows (or
    // bottom ones when the box is low): the rest is left for a window
    // over it.
    void boxCommand(int phrase);
    void endBox();
    bool inBox() const { return box_.has_value(); }

    // Starts talking to character `c`, who says the first line there is.
    // `x` and `y` are where the room showed the character, or negative
    // when it did not.
    // `face` is the portrait to show (the character's own when negative)
    // and `variant` which of its sets.
    void start(int c, int x, int y, int face = -1, int variant = 0);
    // On coming into a room: character `c` speaks up when list 4 of its
    // group has a line that may be said now, and the talk starts with it.
    // False, with nothing shown, when there is none.
    bool speakUp(int c, int x, int y, int face = -1, int variant = 0);
    // The answer of the last line that gave one: 0xFF yes. Paul goes
    // through a door when nobody there said no.
    std::uint8_t answer() const { return answer_; }
    void expectYes() { answer_ = 0xFF; }
    // A command of the talk menu. False when it is not one.
    bool command(std::uint16_t action);

    // A companion speaks up on the way, over the scene already on the back
    // buffer: the face, then the first line of the story's list of
    // remarks that may be said. False when there is none.
    bool remark(int c);
    // A chief's message in a vision, over the picture already on the back
    // buffer: character `c` with portrait `face` (set `variant`) comes in
    // a mosaic and says the first line of the story's remarks that may be
    // said. False when there is none. endRemark() ends it.
    bool vision(int c, int face, int variant);
    // A line of one of the story's scenes: character `c` (shown with
    // portrait `face`, set `variant`), zoomed in on where the room shows
    // it (`x`, `y`, or negative), says the first line of its list 7 that
    // may be said. False when there is none. endRemark() ends it.
    bool sceneLine(int c, int x, int y, int face, int variant);
    // A scene's line said with no face, as over the small map of the
    // regions: the voice of the first line of character `c`'s list 7 that
    // may be said.
    bool sceneVoice(int c);
    // A scene's line in a box, as a troop's chief talks when given orders:
    // the first line of list 7 that may be said.
    bool sceneBox(int c, int face, int variant, const Box& box);
    // News in the message room: character `c`, zoomed in on (`x`, `y`)
    // where its hologram stands, says the first line of its list 4 that
    // may be said, with "Viewed" and " WHAT ? " to choose from. False when
    // there is none. endRemark() ends it.
    bool news(int c, int x, int y);
    static constexpr std::uint16_t kViewed = 0x2993;
    // The talk ends without a word, as when a scene takes over.
    void leave();
    // A scene shows character `c` in front of the room, saying nothing.
    void sceneFace(int c, int face, int variant);
    // The flight goes on: the remark is over.
    void endRemark();
    bool remarking() const { return remark_; }

    // The story's own list of lines: run when the story moves on, they say
    // nothing but change the game.
    void runEvents();
    // The story moves on to step `to`, if it is not there yet.
    void raiseProgress(int to);

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

    bool sceneLine_ = false;
    void begin(int c, int x, int y, int face, int variant);
    void prepareCondition();
    bool condition(int n);
    void next();
    void sayLine(std::size_t at, bool events);
    void showPage(const std::string& page);
    void afterLine(std::size_t at);
    void runAction(const DialogueLine& line);
    void speak();
    void recording(int& who, int& number, int& variant);
    void drawBackground(int x, int y);
    void restartFace();
    void drawSubtitle(const ClipRect* clip);
    void showScene();
    void layoutSubtitle(const std::string& text);
    void setMenu();
    void setMenuFor(int number);
    int face_ = -1;
    int faceVariant_ = 0;
    void comeWithMe(bool work);
    void stopFollowing();
    void leaves(std::size_t record);
    void takeSlot(int number);
    void stop();
    void end();
    void showBoxText(const std::string& page);
    void loadFonts();
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
    std::optional<Box> box_;
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
