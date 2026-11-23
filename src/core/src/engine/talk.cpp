// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/talk.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/data/string_table.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/engine/text_box.hpp"
#include "mentat/engine/world.hpp"

#include <algorithm>
#include <cstdio>

namespace mentat {

namespace {

// Resources.
constexpr int kConditionsFile = 0xBC; // CONDIT.HSQ
constexpr int kDialogueFile = 0xBD;   // DIALOGUE.HSQ
constexpr int kFirstPhraseFile = 0x93; // PHRASE11.HSQ; part 2 from 0x9A
constexpr int kSecondPhraseFile = 0x9A;

// The talk menu's phrases.
constexpr std::uint16_t kTalkToMePhrase = 0x9F;
constexpr std::uint16_t kComeWithMePhrase = 0x91;
constexpr std::uint16_t kStopFollowingPhrase = 0x92;
constexpr std::uint16_t kWhatPhrase = 0x95;
constexpr std::uint16_t kStopTalkingPhrase = 0x94;
constexpr std::uint16_t kViewedPhrase = 0xD9;

// The characters' records in the state: 16 bytes each.
constexpr std::size_t kCharacters = 0xFD8;
constexpr std::size_t kRecordSize = 0x10;
constexpr std::size_t kNumber = 0x0E;
constexpr std::size_t kFlags = 0x0F;
constexpr std::size_t kLastWith = 0x08; // the clock when they last joined Paul
constexpr std::size_t kLastLeft = 0x0A; // the clock when they last left him
constexpr std::uint8_t kCannotFollow = 0x80;
constexpr std::uint8_t kFollows = 0x40;
constexpr std::uint8_t kTalkedTo = 0x20;
constexpr std::uint8_t kWaiting = 0x04;

// State the conditions and the lines use.
constexpr std::size_t kClock = 0x02;
constexpr std::size_t kMet = 0x0E;       // a bit for each character Paul has talked to
constexpr std::size_t kMetToo = 0x14;
constexpr std::size_t kWith = 0x10;      // those who go with Paul
constexpr std::size_t kTimeApart = 0x16; // the clock since the character last joined or left Paul
constexpr std::size_t kTalkFlags = 0x18; // the character's flags
constexpr std::size_t kSaidSomething = 0x19;
constexpr std::size_t kAsked = 0x1B;
constexpr std::size_t kWillFollow = 0x23;
constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kProgressDone = 0xFF;
constexpr std::size_t kSpice = 0xAC;
constexpr std::size_t kCharm = 0x29;
constexpr std::size_t kTroops = 0x36;
constexpr std::size_t kExploits = 0xC2;
constexpr std::size_t kVisits = 0xF5;
constexpr std::size_t kFilledIn = 0x11EB; // words a phrase can name, codes 0x81 to 0x8F
constexpr std::size_t kNamedPlace = 0x11DB;
constexpr std::size_t kPlaceName = 0x11ED;
constexpr std::size_t kPlaceKind = 0x11EF;
constexpr int kPlaceKindPhrase = 0x0C;
constexpr std::size_t kSlots = 0x1152; // the two followers shown on the panel
constexpr std::size_t kFirstStoryMark = 0x100B;

// The phrase numbers a phrase names: bit 11 for the phrase files, else the
// command file.
constexpr int kPhraseBit = 0x800;
constexpr int kPageStep = 0x1000;

// Phrase codes.
constexpr std::uint8_t kNamedPhrase = 0x80;
constexpr std::uint8_t kLastFilledIn = 0x8F;
constexpr std::uint8_t kNumberWord = 0x92;
constexpr std::uint8_t kLastNumber = 0x9F;
constexpr std::uint8_t kLastPlain = 0xCF;
constexpr std::uint8_t kTwoBytes = 0xD0;
constexpr std::uint8_t kFourBytes = 0xD1;
constexpr std::uint8_t kLastWithBytes = 0xEF;
constexpr std::uint8_t kPhraseEnd = 0xFF;

// Subtitles below the picture.
constexpr int kSubtitleWidth = 320 - 16 - 16;
constexpr int kSubtitleLeft = 16;
constexpr int kSubtitleTop = 1;
constexpr int kSubtitleBottom = 0x92; // the rows end above here
constexpr std::uint8_t kSubtitleColour = 0x0F;
constexpr std::uint8_t kOutlineColour = 0xF0;
constexpr std::uint8_t kVisionOutline = 0x08;   // while a message is shown
constexpr std::size_t kMessageAbout = 0xEA;
constexpr int kSceneBottom = 152;

// The background: the room enlarged four times around the character.
constexpr int kTalkZoom = 6;
// What each character says in the story's scenes.
constexpr int kSceneList = 7;
constexpr int kZoomRight = 0xF0;
constexpr int kZoomBottom = 0x71;

// The list of the story's own lines, and its list of remarks on the way
// (list 4 of character 16).
constexpr std::size_t kEventList = 135;
constexpr int kEventCharacter = 0x10;
constexpr std::size_t kRemarkList = kEventCharacter * Dialogue::kListsPerCharacter + 4;
// A character's list of what it says when Paul comes in.
constexpr int kSpeakUpList = 4;
// A remark waits this long after the face is shown, and its recordings are
// numbered from here (counting the remarks' phrases from 1).
constexpr int kRemarkDelay = 0x4B;
constexpr int kFirstRemarkRecording = 0x3E7;

// Characters with their own way of talking.
constexpr int kRetriesWithOnceMore = 0x0D;
constexpr int kNoFaceFrom = 0x10;
constexpr int kLastRecordedCharacter = 0x0E;
// A phrase of the command file said in a box: its recording's number, how
// many rows it takes, and how far down it goes in a box low on the screen.
constexpr int kCommandRecordings = 0x10A;
constexpr int kCommandRows = 0x19;
constexpr int kLowBoxText = 0x32;
constexpr int kLowBoxShift = 0x26;
constexpr int kCaptain = 12;
constexpr int kChiefs = 15;
constexpr std::size_t kTalking = 0x11CA; // set once Paul has talked to someone
constexpr std::size_t kCharacterTable = 0xFD8;
constexpr std::size_t kCharacterSize = 0x10;
constexpr std::size_t kPresentWord = 0x12;
constexpr std::size_t kCaptainHeld = 0x10A7; // the captain's flags: bit 4, overpowered
constexpr std::size_t kStoryFlags = 0x0A;    // bit 4: the Fremen rally to Paul
constexpr std::uint16_t kOverpowerPhrase = 0x9C;
constexpr std::uint16_t kGiveOrdersPhrase = 0x93;
constexpr std::uint16_t kWorkForMePhrase = 0x96; // and 0x97, "RALLY ME"

constexpr int kEntranceHall = 1;

std::uint16_t bit(int c) { return static_cast<std::uint16_t>(1u << (c & 15)); }

std::string decimal(unsigned value) {
    char text[8];
    std::snprintf(text, sizeof text, "%u", value);
    return text;
}

} // namespace

int Talk::characterOf(std::uint16_t action) {
    // The people of a place's troops.
    switch (action) {
    case 0x932E: return 12;
    case 0x936F: return 13;
    case 0x9373: return 14;
    case 0x937E: return 15;
    default: break;
    }
    const int d = action - kFirstCharacter;
    if (d < 0 || d % kTalkActionStep != 0 || d / kTalkActionStep >= kTalkActions) {
        return -1;
    }
    return d / kTalkActionStep;
}

Talk::Talk(Game& game, World& world, Panel& panel, TalkHost& host, int language)
    : game_(game), world_(world), panel_(panel), host_(host), language_(language),
      dialogue_(Dialogue::parse(game.resources().load(kDialogueFile))),
      conditions_(Conditions::parse(game.resources().load(kConditionsFile))) {
    world_.storyEvents = [this] { runEvents(); };
}

const PhraseFile& Talk::phrases(bool second) {
    auto& slot = phrases_[second ? 1 : 0];
    if (!slot) {
        const int file = (second ? kSecondPhraseFile : kFirstPhraseFile) + language_;
        slot = PhraseFile::parse(game_.resources().load(file));
    }
    return *slot;
}

std::uint16_t Talk::record() const {
    return static_cast<std::uint16_t>(kCharacters + kRecordSize * static_cast<std::size_t>(character_ & 15));
}

// --- Choosing what to say

// Before each search the original puts some of the character's record
// where the conditions can read it.
void Talk::prepareCondition() {
    if (character_ < kNoFaceFrom) {
        const std::size_t r = record();
        const std::uint8_t flags = world_.u8(r + kFlags);
        world_.setU8(kTalkFlags, flags);
        const std::uint16_t since = world_.u16(r + ((flags & kFollows) ? kLastWith : kLastLeft));
        world_.setU16(kTimeApart, static_cast<std::uint16_t>(world_.u16(kClock) - since));
    }
    if (world_.u8(kProgress) < 100 && character_ < 9) {
        const std::uint16_t place = world_.u16(kNamedPlace);
        if (place != 0) {
            world_.setU16(kPlaceName, world_.u8(place));
            world_.setU16(kPlaceKind, static_cast<std::uint16_t>(world_.u8(place + 1u) + kPlaceKindPhrase));
        }
    }
}

bool Talk::condition(int n) { return conditions_.test(n, world_); }

void Talk::start(int c, int x, int y, int face, int variant) {
    begin(c, x, y, face, variant);
    setMenu();
    next();
}

bool Talk::speakUp(int c, int x, int y, int face, int variant) {
    const auto list = dialogue_.list(static_cast<std::size_t>(c * Dialogue::kListsPerCharacter + kSpeakUpList));
    if (!list) {
        return false;
    }
    character_ = c;
    prepareCondition();
    // As with remarks, a line said before may be said again unless it has
    // bit 5.
    const auto found = dialogue_.find(*list, DialogueLine::kOnceMore, [this](int n) { return condition(n); });
    if (!found) {
        character_ = -1;
        return false;
    }
    begin(c, x, y, face, variant);
    setMenu();
    sayLine(*found, false);
    // The talk goes on from the group's first list.
    resume_.reset();
    finished_ = false;
    return true;
}

void Talk::begin(int c, int x, int y, int face, int variant) {
    character_ = c;
    face_ = face < 0 ? c : face;
    faceVariant_ = variant;
    world_.setU8(kTalking, 1);
    host_.showPointer(false);
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = [this](const ClipRect& dirty) {
        if (subtitle_.shown && dirty.bottom > subtitle_.top) {
            drawSubtitle(&dirty);
        }
    };
    subtitle_.shown = false;
    drawBackground(x, y);
    talker.show(face_, 0, faceVariant_, game_.textWords);
    host_.keepOrnamentBackground();
    showScene();

    // The talk starts: the character has met Paul now.
    world_.setU16(kMet, static_cast<std::uint16_t>(world_.u16(kMet) | bit(c)));
    world_.setU16(kMetToo, static_cast<std::uint16_t>(world_.u16(kMetToo) | bit(c)));
    topic_ = c * Dialogue::kListsPerCharacter;
    resume_.reset();
    finished_ = false;
    pages_.clear();
    skipSaid_ = DialogueLine::kSaid;
    world_.setU8(kSaidSomething, 0);
}

// The menu: what Paul can say. Whether "come with me" is on offer, or
// "stop following me", goes by the character's flags.
void Talk::setMenu() { setMenuFor(world_.u8(record() + kNumber)); }

void Talk::setMenuFor(int number) {
    const std::size_t r = kCharacterTable + kCharacterSize * static_cast<std::size_t>(number);
    const std::uint8_t flags = world_.u8(r + kFlags);
    Command follow{static_cast<std::uint16_t>(kComeWithMePhrase | Command::kGrey), kComeWithMe};
    if (number != kCaptain && (world_.u16(kPresentWord) & bit(kCaptain)) && !(world_.u8(kCaptainHeld) & 0x10)) {
        follow = Command{kOverpowerPhrase, kOverpower};
    } else if (number == kChiefs) {
        follow = Command{kGiveOrdersPhrase, kGiveOrders};
    } else if (number == kLastRecordedCharacter) {
        follow = Command{static_cast<std::uint16_t>(kWorkForMePhrase + ((world_.u8(kStoryFlags) & 0x10) ? 1 : 0)),
                         kWorkForMe};
    } else if (!(flags & kCannotFollow)) {
        follow.phrase = kComeWithMePhrase;
        if (flags & kFollows) {
            follow = Command{kStopFollowingPhrase, kStopFollowing};
        }
    }
    panel_.setCommands({Command{kTalkToMePhrase, kTalkToMe}, follow, Command{kWhatPhrase, kWhat},
                        Command{kStopTalkingPhrase, kStopTalking}});
    host_.menuChanged();
    Display& d = game_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
}

bool Talk::command(std::uint16_t action) {
    if (!active() || box_) {
        return false;
    }
    if (remark_) {
        // Only " WHAT ? " is the talk's: the rest goes back to the flight.
        if (action == kWhat) {
            speak();
            return true;
        }
        return false;
    }
    switch (action) {
    case kTalkToMe: next(); return true;
    case kComeWithMe: comeWithMe(false); return true;
    case kWorkForMe: comeWithMe(true); return true;
    case kGiveOrders:
        if (ordersAsked) {
            end();
            ordersAsked();
        }
        return true;
    case kOverpower: return true;  // not built yet
    case kStopFollowing: stopFollowing(); return true;
    case kWhat:
        speak();
        return true;
    case kStopTalking: stop(); return true;
    default: return false;
    }
}

// The next line: the rest of a long one, or the first line of the
// character's lists that may be said now. When the four lists of the
// group have nothing more, the talk ends.
void Talk::next() {
    world_.setU8(kAsked, 0);
    if (!pages_.empty()) {
        phrase_ += kPageStep;
        const std::string page = pages_.front();
        pages_.erase(pages_.begin());
        showPage(page);
        afterLine(pageLine_);
        return;
    }
    for (;;) {
        std::optional<std::size_t> from;
        if (!finished_) {
            from = resume_ ? resume_ : dialogue_.list(static_cast<std::size_t>(topic_));
        }
        if (from) {
            prepareCondition();
            const auto found = dialogue_.find(*from, skipSaid_, [this](int n) { return condition(n); });
            if (found) {
                sayLine(*found, false);
                return;
            }
            resume_.reset();
            ++topic_;
            if ((topic_ & 3) != 0) {
                continue;
            }
        }
        if (character_ != kRetriesWithOnceMore || skipSaid_ == DialogueLine::kOnceMore) {
            stop();
            return;
        }
        topic_ &= ~7;
        finished_ = false;
        resume_.reset();
        skipSaid_ = DialogueLine::kOnceMore;
    }
}

void Talk::sayLine(std::size_t at, bool events) {
    const DialogueLine line = dialogue_.line(at);
    lineFlags_ = line.flags;
    take_ = 0;
    mode_ = subtitles;
    if (box_) {
        mode_ = Subtitles::Below;
    } else if (!events && character_ < kNoFaceFrom) {
        if (mode_ == Subtitles::None && (line.extra & DialogueLine::kTextOnly)) {
            mode_ = Subtitles::Balloons; // there is no recording of it
        }
        if (mode_ == Subtitles::Below) {
            host_.closeOrnament();
        }
    }
    phrase_ = line.phrase | kPhraseBit;
    secondFile_ = dialogue_.inSecondFile(at);
    pages_.clear();
    if (!events) {
        std::vector<std::string> pages = phraseText(phrase_, secondFile_);
        showPage(pages.front());
        pages_.assign(pages.begin() + 1, pages.end());
    }
    pageLine_ = at;
    afterLine(at);
}

// After a page is shown: once the last page of a line is, the line counts
// as said and does what it does.
void Talk::afterLine(std::size_t at) {
    std::optional<std::size_t> next = at;
    bool ended = false;
    if (pages_.empty()) {
        const DialogueLine line = dialogue_.line(at);
        if (line.action() != 0) {
            runAction(line);
        }
        if ((line.extra & DialogueLine::kNoteMask) && !line.said()) {
            // The original notes the line down for later.
            notes_.push_back(static_cast<std::uint16_t>(((at - 2) >> 2) | (static_cast<std::size_t>(character_) << 11)));
        }
        world_.setU8(kSaidSomething, 0xFF);
        dialogue_.markSaid(at);
        next = at + 4;
        if (endAfter_) {
            endAfter_ = false;
            ended = true;
        }
    }
    if (character_ >= 0 && character_ < kNoFaceFrom && !box_) {
        Talker& talker = game_.talker();
        talker.restart(words_);
        if (subtitle_.shown) {
            drawSubtitle(nullptr);
        }
        showScene();
        host_.showPointer(false);
        speak();
        host_.showPointer(true);
    }
    resume_ = next;
    finished_ = ended;
}

// --- What lines do

void Talk::runAction(const DialogueLine& line) {
    switch (line.action()) {
    case 1: answer_ = 0xFF; break; // yes
    case 2: answer_ = 0; break;    // no
    case 6: endAfter_ = true; break;
    case 7: answer_ = 0x80; break;
    case 11:
        if (!line.said()) {
            // One step on: the story's lines run, but no step's own changes.
            world_.setU8(kProgress, static_cast<std::uint8_t>(world_.u8(kProgress) + 1));
            world_.setU8(kProgressDone, 0);
            runEvents();
            if (world_.u8(kProgress) == 1) {
                world_.setU8(kFirstStoryMark, 1);
            }
        }
        break;
    case 12:
        if (!line.said()) {
            raiseProgress((world_.u8(kProgress) & 0xFC) + 4);
        }
        break;
    case 13: {
        // One of the story's scenes, by how far the story has come.
        const int step = world_.u8(kProgress);
        world_.scene = step < 0x14 ? 0x12F8 : step < 0x18 ? 0x134F : step < 0x30 ? 0x1370 : 0x12DB;
        break;
    }
    case 14:
        if (!line.said()) {
            world_.setU8(kExploits, static_cast<std::uint8_t>(world_.u8(kExploits) + 1));
        }
        break;
    case 15:
        if (character_ == 1) {
            world_.setU8(kVisits, static_cast<std::uint8_t>(world_.u8(kVisits) + 1));
        }
        break;
    default:
        // The others start things this port does not have yet (messages,
        // choices of troops, the story's own steps).
        break;
    }
}

// The story moves on: the new step, its lines run, and what the step
// changes (see World::raiseStory).
void Talk::raiseProgress(int to) {
    world_.raiseStory(to);
}

void Talk::runEvents() {
    const int saved = character_;
    const std::optional<std::size_t> savedResume = resume_;
    const bool savedFinished = finished_;
    const auto savedPages = pages_;
    const std::size_t savedPageLine = pageLine_;
    character_ = kEventCharacter;
    if (const auto list = dialogue_.list(kEventList)) {
        prepareCondition();
        if (const auto found = dialogue_.find(*list, DialogueLine::kSaid, [this](int n) { return condition(n); })) {
            sayLine(*found, true);
        }
    }
    character_ = saved;
    resume_ = savedResume;
    finished_ = savedFinished;
    pages_ = savedPages;
    pageLine_ = savedPageLine;
}

// --- Remarks on the way

bool Talk::remark(int c) {
    character_ = c;
    remark_ = true;
    host_.showPointer(false);
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = [this](const ClipRect& dirty) {
        if (subtitle_.shown && dirty.bottom > subtitle_.top) {
            drawSubtitle(&dirty);
        }
    };
    subtitle_.shown = false;
    drawBackground(-1, -1);
    talker.show(c, 0, 0, game_.textWords);
    host_.keepOrnamentBackground();
    showScene();
    host_.showPointer(true);
    game_.waitTicks(kRemarkDelay);

    pages_.clear();
    resume_.reset();
    finished_ = false;
    const auto list = dialogue_.list(kRemarkList);
    if (!list) {
        return false;
    }
    prepareCondition();
    // A remark said before may be said again, unless it has bit 5.
    const auto found = dialogue_.find(*list, DialogueLine::kOnceMore, [this](int n) { return condition(n); });
    if (!found) {
        return false;
    }
    sayLine(*found, false);
    return true;
}

bool Talk::vision(int c, int face, int variant) {
    character_ = c;
    remark_ = true;
    host_.showPointer(false);
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = [this](const ClipRect& dirty) {
        if (subtitle_.shown && dirty.bottom > subtitle_.top) {
            drawSubtitle(&dirty);
        }
    };
    subtitle_.shown = false;
    drawBackground(-1, -1);
    talker.show(face, 0, variant, game_.textWords);
    game_.display().transition(Display::kMosaic);
    host_.showPointer(true);

    pages_.clear();
    resume_.reset();
    finished_ = false;
    const auto list = dialogue_.list(kRemarkList);
    if (!list) {
        return false;
    }
    prepareCondition();
    const auto found = dialogue_.find(*list, DialogueLine::kOnceMore, [this](int n) { return condition(n); });
    if (!found) {
        return false;
    }
    sayLine(*found, false);
    return true;
}

void Talk::sceneFace(int c, int face, int variant) {
    endRemark();
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = nullptr;
    subtitle_.shown = false;
    drawBackground(-1, -1);
    talker.show(face < 0 ? c : face, 0, variant, game_.textWords);
    host_.keepOrnamentBackground();
    showScene();
    talker.halt();
}

bool Talk::sceneLine(int c, int x, int y, int face, int variant) {
    endRemark();
    character_ = c;
    remark_ = true;
    sceneLine_ = true;
    host_.showPointer(false);
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = [this](const ClipRect& dirty) {
        if (subtitle_.shown && dirty.bottom > subtitle_.top) {
            drawSubtitle(&dirty);
        }
    };
    subtitle_.shown = false;
    drawBackground(x, y);
    talker.show(face < 0 ? c : face, 0, variant, game_.textWords);
    host_.keepOrnamentBackground();
    showScene();
    host_.showPointer(true);

    pages_.clear();
    resume_.reset();
    finished_ = false;
    const auto list = dialogue_.list(static_cast<std::size_t>(c * Dialogue::kListsPerCharacter + kSceneList));
    if (!list) {
        return false;
    }
    prepareCondition();
    const auto found = dialogue_.find(*list, DialogueLine::kSaid, [this](int n) { return condition(n); });
    if (!found) {
        return false;
    }
    sayLine(*found, false);
    return true;
}

bool Talk::sceneVoice(int c) {
    endRemark();
    character_ = c;
    remark_ = true;
    sceneLine_ = true;
    game_.talker().halt();
    subtitle_.shown = false;
    pages_.clear();
    resume_.reset();
    finished_ = false;
    const auto list = dialogue_.list(static_cast<std::size_t>(c * Dialogue::kListsPerCharacter + kSceneList));
    if (!list) {
        return false;
    }
    prepareCondition();
    const auto found = dialogue_.find(*list, DialogueLine::kSaid, [this](int n) { return condition(n); });
    if (!found) {
        return false;
    }
    const DialogueLine line = dialogue_.line(*found);
    lineFlags_ = line.flags;
    take_ = 0;
    phrase_ = line.phrase | kPhraseBit;
    secondFile_ = dialogue_.inSecondFile(*found);
    pageLine_ = *found;
    if (subtitles == Subtitles::None) {
        int who = 0;
        int number = 0;
        int variant = 0;
        recording(who, number, variant);
        game_.talker().playVoice(who, number, variant);
    }
    afterLine(*found);
    return true;
}

bool Talk::sceneBox(int c, int face, int variant, const Box& box) {
    endRemark();
    startBox(c, face, variant, box);
    const auto list = dialogue_.list(static_cast<std::size_t>(c * Dialogue::kListsPerCharacter + kSceneList));
    if (!list) {
        return false;
    }
    prepareCondition();
    const auto found = dialogue_.find(*list, DialogueLine::kSaid, [this](int n) { return condition(n); });
    if (!found) {
        return false;
    }
    sayLine(*found, false);
    return true;
}

bool Talk::news(int c, int x, int y) {
    endRemark();
    character_ = c;
    remark_ = true;
    host_.showPointer(false);
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = [this](const ClipRect& dirty) {
        if (subtitle_.shown && dirty.bottom > subtitle_.top) {
            drawSubtitle(&dirty);
        }
    };
    subtitle_.shown = false;
    drawBackground(x, y);
    talker.show(c, 0, 0, game_.textWords);
    host_.keepOrnamentBackground();
    showScene();
    host_.showPointer(true);
    panel_.setCommands({Command{kViewedPhrase, kViewed}, Command{kWhatPhrase, kWhat}});
    host_.menuChanged();
    Display& d = game_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();

    pages_.clear();
    resume_.reset();
    finished_ = false;
    const auto list = dialogue_.list(static_cast<std::size_t>(c * Dialogue::kListsPerCharacter + kSpeakUpList));
    if (!list) {
        return false;
    }
    prepareCondition();
    const auto found = dialogue_.find(*list, DialogueLine::kOnceMore, [this](int n) { return condition(n); });
    if (!found) {
        return false;
    }
    sayLine(*found, false);
    return true;
}

void Talk::endRemark() {
    if (!remark_) {
        return;
    }
    const std::size_t r = record();
    if (!sceneLine_) {
        world_.setU8(r + kFlags, static_cast<std::uint8_t>((world_.u8(r + kFlags) | kTalkedTo) & ~kWaiting));
    }
    sceneLine_ = false;
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = nullptr;
    subtitle_.shown = false;
    pages_.clear();
    character_ = -1;
    remark_ = false;
}

// "Come with me", or the Fremen's "work for me": the character answers
// from list 5 of the group and goes with Paul, or the troop works for him,
// when the answer is yes.
void Talk::comeWithMe(bool work) {
    if (work) {
        std::uint8_t willing = 0;
        if (world_.u16(kSpice) >= 1000) {
            const int left = 100 - world_.u8(kCharm);
            if (left >= 0 && (left >> 2) > world_.u8(kTroops)) {
                willing = 2;
            }
        }
        world_.setU8(kWillFollow, willing);
    }
    answer_ = 0xFF;
    const auto savedResume = resume_;
    const bool savedFinished = finished_;
    const std::uint8_t savedSkip = skipSaid_;
    skipSaid_ = DialogueLine::kOnceMore;
    prepareCondition();
    if (const auto list = dialogue_.list(static_cast<std::size_t>((topic_ & ~7) + 5))) {
        if (const auto found = dialogue_.find(*list, skipSaid_, [this](int n) { return condition(n); })) {
            pages_.clear();
            sayLine(*found, false);
        }
    }
    skipSaid_ = savedSkip;
    resume_ = savedResume;
    finished_ = savedFinished;
    world_.setU8(kAsked, static_cast<std::uint8_t>(world_.u8(kAsked) + 1));
    world_.setU8(kWillFollow, 0);
    if (answer_ != 0xFF) {
        return;
    }
    const std::size_t r = record();
    const int number = world_.u8(r + kNumber);
    if (number == kLastRecordedCharacter) {
        if (enlisted) {
            enlisted();
        }
        setMenuFor(kChiefs);
        return;
    }
    world_.setU8(r + kFlags, static_cast<std::uint8_t>(world_.u8(r + kFlags) | kFollows));
    if (static_cast<std::uint16_t>(world_.u16(kClock) - world_.u16(r + kLastLeft)) >= 2) {
        world_.setU16(r + kLastWith, world_.u16(kClock));
    }
    world_.setU16(kWith, static_cast<std::uint16_t>(world_.u16(kWith) | bit(number)));
}

// "Stop following me": list 6 of the group, and the character stays.
void Talk::stopFollowing() {
    answer_ = 0xFF;
    const auto savedResume = resume_;
    const bool savedFinished = finished_;
    const std::uint8_t savedSkip = skipSaid_;
    skipSaid_ = DialogueLine::kOnceMore;
    prepareCondition();
    if (const auto list = dialogue_.list(static_cast<std::size_t>((topic_ & ~7) + 6))) {
        if (const auto found = dialogue_.find(*list, skipSaid_, [this](int n) { return condition(n); })) {
            pages_.clear();
            sayLine(*found, false);
        }
    }
    skipSaid_ = savedSkip;
    resume_ = savedResume;
    finished_ = savedFinished;
    world_.setU8(kAsked, static_cast<std::uint8_t>(world_.u8(kAsked) + 1));
    if (answer_ != 0xFF) {
        return;
    }
    leaves(record());
}

// A character stops going with Paul.
void Talk::leaves(std::size_t r) {
    world_.setU8(r + kFlags, static_cast<std::uint8_t>(world_.u8(r + kFlags) & ~kFollows));
    if (static_cast<std::uint16_t>(world_.u16(kClock) - world_.u16(r + kLastWith)) >= 2) {
        world_.setU16(r + kLastLeft, world_.u16(kClock));
    }
    const int number = world_.u8(r + kNumber);
    world_.setU16(kWith, static_cast<std::uint16_t>(world_.u16(kWith) & ~bit(number)));
}

// A follower takes a slot of the panel; when both are taken, the first
// one leaves Paul to make room.
void Talk::takeSlot(int number) {
    const std::uint8_t first = world_.u8(kSlots);
    const std::uint8_t second = world_.u8(kSlots + 1);
    if (first == number || second == number) {
        return;
    }
    if (first == 0xFF) {
        world_.setU8(kSlots, static_cast<std::uint8_t>(number));
        return;
    }
    if (second == 0xFF) {
        world_.setU8(kSlots + 1, static_cast<std::uint8_t>(number));
        return;
    }
    leaves(kCharacters + kRecordSize * first);
    world_.setU8(kSlots, second);
    world_.setU8(kSlots + 1, static_cast<std::uint8_t>(number));
}

void Talk::stop() {
    if (!active()) {
        return;
    }
    end();
    host_.backToRoom();
}

// The talk is over: the character was talked to and, going with Paul,
// takes a slot.
void Talk::leave() {
    if (remark_) {
        endRemark();
    } else if (active() && !box_) {
        end();
    }
}

void Talk::end() {
    const std::size_t r = record();
    world_.setU8(r + kFlags, static_cast<std::uint8_t>((world_.u8(r + kFlags) | kTalkedTo) & ~kWaiting));
    if (world_.u8(r + kFlags) & kFollows) {
        takeSlot(world_.u8(r + kNumber));
    }
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = nullptr;
    subtitle_.shown = false;
    pages_.clear();
    character_ = -1;
}

// --- Talking in a box

void Talk::startBox(int c, int face, int variant, const Box& box) {
    character_ = c;
    box_ = box;
    remark_ = false;
    Talker& talker = game_.talker();
    talker.halt();
    talker.overlay = nullptr;
    subtitle_.shown = false;
    talker.showInBox(face, variant, game_.textWords, box.face, box.focusX, box.focusY);
    topic_ = c * Dialogue::kListsPerCharacter;
    resume_.reset();
    finished_ = false;
    pages_.clear();
    skipSaid_ = DialogueLine::kOnceMore;
}

bool Talk::boxLine(int list) {
    if (!box_) {
        return false;
    }
    if (!pages_.empty()) {
        phrase_ += kPageStep;
        const std::string page = pages_.front();
        pages_.erase(pages_.begin());
        showBoxText(page);
        afterLine(pageLine_);
        return true;
    }
    const auto first = dialogue_.list(static_cast<std::size_t>(character_ * Dialogue::kListsPerCharacter + list));
    if (!first) {
        return false;
    }
    prepareCondition();
    auto found = resume_ ? dialogue_.find(*resume_, DialogueLine::kOnceMore, [this](int n) { return condition(n); })
                         : std::nullopt;
    if (!found) {
        found = dialogue_.find(*first, DialogueLine::kOnceMore, [this](int n) { return condition(n); });
    }
    if (!found) {
        resume_.reset();
        return false;
    }
    sayLine(*found, false);
    return true;
}

bool Talk::boxAnswer(int list) {
    pages_.clear();
    resume_.reset();
    const bool said = boxLine(list);
    resume_.reset();
    return said;
}

void Talk::boxCommand(int phrase) {
    if (!box_) {
        return;
    }
    pages_.clear();
    resume_.reset();
    // Numbered as the character's recording of it: 0x10A on from the
    // phrase, counted from the character's first.
    const int who = std::min(character_, kLastRecordedCharacter);
    phrase_ = phrase + kCommandRecordings + dialogue_.firstPhrase(who) - 1;
    loadFonts();
    // The whole text area is cleared; the phrase takes its top rows, or
    // its bottom ones when the box is low on the screen.
    Display& d = game_.display();
    const ClipRect& t = box_->text;
    ClipRect area = t;
    area.top -= 1; // a row higher than a line of the box
    if (t.top >= kLowBoxText) {
        area.top += kLowBoxShift;
    }
    area.bottom = area.top + kCommandRows;
    const std::string& text = panel_.phrases().at(static_cast<std::size_t>(phrase) - 1u);
    for (Surface* to : {&d.screen(), &d.back()}) {
        to->fillRect(t.left, t.top, t.right - t.left, t.bottom - t.top, box_->fill);
        drawTextBox(*to, *large_, text, area, box_->colour);
    }
}

void Talk::endBox() {
    if (!box_) {
        return;
    }
    Talker& talker = game_.talker();
    talker.halt();
    box_.reset();
    pages_.clear();
    resume_.reset();
    character_ = -1;
}

// A page of a line in the box: the text area is cleared and the text laid
// out in it, on the screen and the back buffer.
void Talk::loadFonts() {
    if (!large_) {
        const auto bytes = game_.resources().load(resource::kFont);
        large_ = Font::parse(bytes, Font::Size::Large);
        small_ = Font::parse(bytes, Font::Size::Small);
    }
}

void Talk::showBoxText(const std::string& page) {
    loadFonts();
    Display& d = game_.display();
    for (Surface* to : {&d.screen(), &d.back()}) {
        to->fillRect(box_->text.left, box_->text.top, box_->text.right - box_->text.left,
                     box_->text.bottom - box_->text.top, box_->fill);
        drawTextBox(*to, *large_, page, box_->text, box_->colour);
    }
    words_ = 0;
}

// --- The voice

void Talk::speak() {
    if (mode_ == Subtitles::Below || character_ < 0 || character_ >= kNoFaceFrom) {
        return;
    }
    int who = 0;
    int number = 0;
    int variant = 0;
    recording(who, number, variant);
    game_.talker().speak(number, variant, remark_ && !sceneLine_ ? -1 : who);
}

// Whose recording says the line said last, and which.
void Talk::recording(int& who, int& number, int& variant) {
    // The recordings count each character's phrases from its first; the
    // remarks are counted from the first of the story's lines, after 999.
    who = std::min(character_, kLastRecordedCharacter);
    number = remark_ && !sceneLine_ ? (phrase_ & ~0x0C00) - (dialogue_.firstPhrase(kEventCharacter) - 1) + kFirstRemarkRecording
                                    : (phrase_ & ~0x0C00) - (dialogue_.firstPhrase(who) - 1);
    if (who == kLastRecordedCharacter && (number & 0xFE) == 0x2C) {
        who = 0x0C;
    }
    game_.indoors = world_.inside() && (world_.room() & 0xFF) != kEntranceHall;
    variant = ((number >> 12) & 0x0F) | take_;
    number &= 0xFFF;
}

// --- Text

std::vector<std::string> Talk::phraseText(int phrase, bool secondFile) {
    const int index = (phrase - 1) & 0x7FF;
    const std::string raw = phrases(secondFile).text(static_cast<std::size_t>(index));
    std::vector<std::string> pages;
    std::string out;
    expand(raw, out, pages, 0, secondFile);
    pages.push_back(out);
    if (lineFlags_ & DialogueLine::kRandomTake) {
        take_ = game_.random(3);
        lineFlags_ = static_cast<std::uint8_t>(lineFlags_ & ~DialogueLine::kRandomTake);
    }
    return pages;
}

// Puts in what a phrase names: other phrases, words the game keeps and
// numbers from the state. A page break at the top level starts a new page.
void Talk::expand(const std::string& in, std::string& out, std::vector<std::string>& pages, int depth, bool secondFile) {
    if (depth > 8) {
        throw FormatError("phrases: phrases name each other");
    }
    std::size_t i = 0;
    auto next = [&]() -> std::uint8_t {
        if (i >= in.size()) {
            throw FormatError("phrases: code without its bytes");
        }
        return static_cast<std::uint8_t>(in[i++]);
    };
    while (i < in.size()) {
        const std::uint8_t b = next();
        if (b < kNamedPhrase) {
            out.push_back(static_cast<char>(b));
        } else if (b <= kLastFilledIn) {
            int named = 0;
            if (b == kNamedPhrase) {
                const std::uint8_t hi = next();
                named = (hi << 8) | next();
            } else {
                named = world_.u16(kFilledIn + 2 * static_cast<std::size_t>(b & 0x0F));
            }
            chooseTake(b, named);
            const int index = named - 1;
            std::string text;
            if (index & kPhraseBit) {
                text = phrases(secondFile).raw(static_cast<std::size_t>(index & 0x7FF));
            } else {
                text = panel_.phrases().at(static_cast<std::size_t>(index & 0x7FF));
            }
            // A named phrase ends at its first code from 0xF0 on.
            std::string inner;
            for (char c : text) {
                if (static_cast<std::uint8_t>(c) >= 0xF0) {
                    break;
                }
                inner.push_back(c);
            }
            std::vector<std::string> ignored;
            expand(inner, out, ignored, depth + 1, secondFile);
        } else if (b <= kLastNumber) {
            const std::uint8_t at = next();
            const unsigned value = b == kNumberWord ? world_.u16(at) : world_.u8(at);
            chooseTakeForNumber(at, value);
            out += decimal(value);
        } else if (b <= kLastPlain) {
            out.push_back(static_cast<char>(b));
        } else if (b <= kLastWithBytes) {
            out.push_back(static_cast<char>(b));
            const int extra = b == kTwoBytes ? 2 : b == kFourBytes ? 4 : 1;
            for (int k = 0; k < extra; ++k) {
                out.push_back(static_cast<char>(next()));
            }
        } else if (depth == 0 && b != kPhraseEnd) {
            pages.push_back(out);
            out.clear();
        } else {
            break;
        }
    }
}

// Some lines have a few recordings, one for each word or number the line
// can name.
void Talk::chooseTake(std::uint8_t code, int named) {
    if (!(lineFlags_ & DialogueLine::kRandomTake)) {
        return;
    }
    const int action = lineFlags_ & DialogueLine::kActionMask;
    int t = -1;
    switch (code) {
    case 0x8B: t = (named - 0x108) & 0xFF; break;
    case 0x84:
        if (named == 0x48) {
            t = 0;
        } else if (((named - 0x49) & 0xFF) < 3) {
            t = (named - 0x49) & 0xFF;
        } else if (named - 0x49 + 0x31 < 0x0C) {
            t = named - 0x49 + 0x31;
        }
        break;
    case 0x83:
    case 0x8C:
        if (((named - 0xE8) & 0xFF) < 7) {
            t = (named - 0xE8) & 0xFF;
        }
        break;
    case 0x86:
    case 0x87:
    case 0x88:
        if (action == 1 && ((named - 0xD1) & 0xFF) < 7) {
            t = (named - 0xD1) & 0xFF;
        }
        break;
    case 0x85:
        if (action == 1) {
            t = named == 0x74 ? 1 : 0;
        }
        break;
    case 0x89:
        if (((named - 0xDA) & 0xFF) < 8) {
            t = (named - 0xDA) & 0xFF;
        }
        break;
    default: break;
    }
    if (t >= 0) {
        take_ = t;
        lineFlags_ = static_cast<std::uint8_t>(lineFlags_ & ~DialogueLine::kRandomTake);
    }
}

void Talk::chooseTakeForNumber(std::uint8_t at, unsigned value) {
    if (!(lineFlags_ & DialogueLine::kRandomTake)) {
        return;
    }
    int t = -1;
    if (at == 0xCF) {
        t = static_cast<int>(std::min(value & 0xFF, 8u));
    } else if (at == 0x55 || at == 0x61) {
        t = static_cast<int>(std::min((value - 1) & 0xFF, 4u));
    } else if (at == 0x44 && (lineFlags_ & DialogueLine::kActionMask) == 1) {
        t = static_cast<int>(std::min((value - 1) & 0xFF, 8u));
    }
    if (t >= 0) {
        take_ = t;
        lineFlags_ = static_cast<std::uint8_t>(lineFlags_ & ~DialogueLine::kRandomTake);
    }
}

// Shows a page of a line: the old text goes, and the new one is laid out
// below the picture, unless only the voice is wanted.
void Talk::showPage(const std::string& page) {
    if (box_) {
        showBoxText(page);
        return;
    }
    Display& d = game_.display();
    if (subtitle_.shown) {
        // The scene comes back whole, with the face, before the new text.
        subtitle_.shown = false;
        d.copyRect(d.saved(), d.back(), ClipRect{0, 0, 320, kSceneBottom});
        game_.talker().redraw();
        showScene();
    }
    if (mode_ == Subtitles::None) {
        return;
    }
    layoutSubtitle(page);
}

void Talk::layoutSubtitle(const std::string& text) {
    if (!large_) {
        const auto bytes = game_.resources().load(resource::kFont);
        large_ = Font::parse(bytes, Font::Size::Large);
        small_ = Font::parse(bytes, Font::Size::Small);
    }
    const std::vector<TextLine> lines = layoutText(*large_, text, kSubtitleWidth, &*small_);
    words_ = 0;
    for (const TextLine& l : lines) {
        words_ += l.words;
    }
    Surface& px = subtitle_.pixels;
    px.fill(0);
    if (lines.empty()) {
        subtitle_.shown = false;
        return;
    }
    const int rows = std::min(drawTextLines(px, *large_, text, lines, kSubtitleLeft, kSubtitleTop, kSubtitleColour, &*small_),
                              px.height() - 1);
    // Every pixel of the text gets a dark edge where it touches nothing.
    std::uint8_t* data = px.data();
    const std::size_t end = static_cast<std::size_t>(rows) * 320;
    const std::uint8_t outline =
        static_cast<std::int8_t>(world_.u8(kMessageAbout)) > 0 ? kVisionOutline : kOutlineColour;
    const auto edge = [&](std::ptrdiff_t at) {
        if (at >= 0 && static_cast<std::size_t>(at) < px.size() && data[at] == 0) {
            data[at] = outline;
        }
    };
    for (std::size_t at = 0; at < end; ++at) {
        if (data[at] != kSubtitleColour) {
            continue;
        }
        const auto p = static_cast<std::ptrdiff_t>(at);
        edge(p - 1);
        edge(p + 1);
        edge(p - 320);
        edge(p + 320);
    }
    subtitle_.rows = rows;
    subtitle_.top = kSubtitleBottom - rows;
    subtitle_.shown = true;
}

// The subtitle goes on the back buffer (only within `clip` when given),
// its pixels of colour 0 letting the picture show through.
void Talk::drawSubtitle(const ClipRect* clip) {
    if (!subtitle_.shown) {
        return;
    }
    Display& d = game_.display();
    Surface& back = d.back();
    ClipRect area{0, subtitle_.top, 320, subtitle_.top + subtitle_.rows};
    if (clip != nullptr) {
        area.left = std::max(area.left, clip->left);
        area.top = std::max(area.top, clip->top);
        area.right = std::min(area.right, clip->right);
        area.bottom = std::min(area.bottom, clip->bottom);
    }
    for (int y = area.top; y < area.bottom; ++y) {
        for (int x = area.left; x < area.right; ++x) {
            const std::uint8_t c = subtitle_.pixels.at(x, y - subtitle_.top);
            if (c != 0) {
                back.set(x, y, c);
            }
        }
    }
}

// The back buffer goes to the screen, the ornament drawn over it first.
void Talk::showScene() {
    Display& d = game_.display();
    host_.drawOrnament();
    d.copyRect(d.back(), d.screen(), ClipRect{0, 0, 320, kSceneBottom});
}

// --- The picture

// The room without its people, four times larger around where the
// character stood, becomes the background the face is drawn on.
void Talk::drawBackground(int x, int y) {
    Display& d = game_.display();
    d.targetBack();
    if (x >= 0 && y >= 0) {
        Places& places = game_.places();
        const std::uint16_t present = places.present;
        const std::uint16_t absent = places.absent;
        std::vector<int> standing = std::move(places.standingList);
        places.standingList.clear();
        places.present = 0;
        places.absent = 0;
        places.drawView();
        places.present = present;
        places.absent = absent;
        places.standingList = std::move(standing);
        d.zoom(kTalkZoom, std::min(x, kZoomRight), std::min(y, kZoomBottom), d.back(), d.saved());
    } else {
        d.saveBackground();
    }
    d.copyRect(d.saved(), d.back(), ClipRect{0, 0, 320, kSceneBottom});
}

} // namespace mentat
