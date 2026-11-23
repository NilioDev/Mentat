// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/play.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/cockpit.hpp"
#include "mentat/engine/flight.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/globe_screen.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/orders.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/saved_games.hpp"
#include "mentat/engine/spice_map.hpp"
#include "mentat/engine/talk.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/engine/text_box.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/font.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace mentat {

namespace {

// Where a new game starts: the throne room of the palace, early in the
// morning of the first day.
constexpr int kStartRoom = 0x200A;
constexpr int kStartPlace = 0x0180;
constexpr int kStartClock = 2;

// The panel's layout while the player walks around.
constexpr std::size_t kWalkLayout = 0x1C36;

// The ornament opens one step every 8 ticks.
constexpr int kOrnamentSteps = 10;
constexpr int kOrnamentTicks = 8;

// Commands the program keeps as (phrase, action) pairs.
constexpr std::size_t kTakeOrnithopter = 0x21DC;
constexpr std::size_t kNewMessages = 0x21E8;
constexpr std::size_t kOldMessages = 0x21EC;
constexpr std::size_t kLookAtMirror = 0x21F0;
constexpr std::size_t kMixer = 0x21F4;
constexpr std::size_t kSeeMap = 0x220C;
constexpr std::size_t kCallWorm = 0x2214;
constexpr std::size_t kMassiveAttack = 0x2218; // and "fight for a whole day" after it
// State the commands depend on.
constexpr std::size_t kAtWar = 0x2B;
constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kMessages = 0xC8;
constexpr std::size_t kMessagesSeen = 0xC9;
constexpr int kWormProgress = 0x4F;
constexpr int kPalace = 1;
constexpr int kEntrance = 1;
// The room that shows the sky through its windows.
constexpr int kSkyRoom = 0x2005;
constexpr int kMessageRoom = 8;
constexpr int kPaulsRoom = 9;
// The palace's ornithopters: a new game gives it one.
constexpr std::size_t kPalaceOrnithopters = 0x100 + 0x15;
constexpr std::uint16_t kFirstPersonPhrase = 0x78;
// The people of the troops (see docs/rules/people.md).
constexpr int kCaptain = 12;
constexpr int kSmuggler = 13;
constexpr int kFremen = 14;
constexpr int kStoryEnd = 0xC8;
constexpr int kGurney = 4;
constexpr int kGurneyLyingFrom = 0x15;
constexpr int kGurneyLyingTo = 0x20;
constexpr int kLyingPerson = 0x11;
constexpr int kLyingDown = 0x35;
constexpr int kCaptainHeld = 0x12;
constexpr std::size_t kCaptainFlags = 0x10A7;
constexpr std::size_t kSpotTurn = 0xC5; // turns where the people stand
// What is happening, for what the characters say: Paul goes for a door
// (and the room it leads to), or has come into a room; and the count of
// rooms come into, which starts again at each.
constexpr std::size_t kSituation = 0x23;
constexpr std::uint8_t kLeaving = 1;
constexpr std::uint8_t kComingIn = 5;
constexpr std::size_t kGoingTo = 0x0C;
constexpr std::size_t kEntries = 0x1A;
constexpr std::size_t kCameFrom = 0x0D;
// Negative while the map is shown.
constexpr std::size_t kMapShown = 0xFB;
constexpr int kChiefs = 15;
constexpr int kProspectorsTroop = 3;
constexpr int kProspectorsNamed = 5; // the story's step from which their chief is named
constexpr std::uint16_t kProspectorsPhrase = 0x8F;
constexpr std::size_t kKeenness = 0x36; // the troop talked to
constexpr int kAllRallied = 0x4C; // the story's step once enough troops work for Paul
// A place's ornithopters, in its record.
constexpr std::size_t kOrnithopters = 0x15;

// Taking off and landing: a step every 20 ticks, with its sound; the sound
// moves on to its next part at step 13, and taking off again at step 26.
constexpr int kTakeOffSound = 6;
constexpr int kLandingSound = 7;
// On the way: the commands when a companion has seen a place (go there,
// what), and in the Harkonnens' zone (change destination, ignore the
// warning, what); two bytes, then (phrase, action) pairs up to a phrase
// of 0. Going on is the same action as the cockpit's "Cancel".
constexpr std::size_t kSightingCommands = 0x1F92;
constexpr std::size_t kZoneCommands = 0x1F9E;
constexpr std::uint16_t kGoOn = 0xD2E2;
constexpr std::size_t kNoArrows = 0x1D1E;
// The ornithopter's cabin, behind a companion's face.
constexpr int kCabinSheet = 0x34; // ORNYCAB.HSQ

// The end of the game. Its reasons: 2, caught by the Harkonnens. Each has
// a phrase of the commands' text from 0xBA on, and a character whose face
// is shown and whose recording 0xFFF says it (none from 0x80), from the
// table at data offset 0x225D.
constexpr int kCaught = 2;
constexpr std::uint16_t kFirstEndPhrase = 0xBA;
constexpr std::size_t kEndFaces = 0x225D;
constexpr int kEndVoice = 0xFFF;
// The clock shows the last day.
constexpr std::uint16_t kEndClock = 0x16C5;
// The videos: Paul's death, then, to start again, another.
constexpr int kDeathVideo = 12;   // DEAD.HNM
constexpr int kRestartVideo = 13; // DEAD2.HNM
// The face's frame: (left, top, right, bottom), its edge's colour and its
// inside's; and the box the words are laid out in.
constexpr std::size_t kEndFrame = 0x2254;
constexpr ClipRect kEndText{88, 3, 288, 64};
constexpr std::uint8_t kEndTextColour = 0xF0;
// The sky flashes three times: from the colours of sky 0x28 back toward
// those of the hour, a step every 3 ticks.
constexpr int kFlashSky = 0x28;
constexpr int kFlashes = 3;
constexpr int kFlashStepTicks = 3;
// The commands: start again, load, leave, what; leaving asks first.
constexpr std::size_t kEndCommands = 0x20A2;
constexpr std::size_t kLeaveCommands = 0x20B4;
constexpr std::uint16_t kRestart = 0x0E47;
constexpr std::uint16_t kLeave = 0x0E3E;
constexpr std::uint16_t kLeaveYes = 0x003A;
constexpr std::uint16_t kLeaveNo = 0xD2E2;
constexpr int kLastOrnithopterStep = 0x20;
constexpr int kOrnithopterStepTicks = 0x14;
constexpr int kSoundStep = 0x0D;
constexpr int kSecondSoundStep = 0x1A;
// The music's moment while flying.
constexpr int kFlyingMoment = 6;

// The clock: ticks to a part of the day.
constexpr std::size_t kTicksPerHour = 0x146E;
// The rectangle of the panel that shows the clock.
constexpr ClipRect kClockArea{6, 181, 30, 198};

// The sky: which picture of the sky file goes with each part of the day,
// and which colours it sets, seen from inside and from outside.
constexpr std::size_t kSkyOfTheHour = 0x2280;
constexpr int kInsideSkyFirst = 73;
constexpr int kInsideSkyCount = 151;
constexpr int kOutsideSkyFirst = 128;
constexpr int kOutsideSkyCount = 80;
constexpr int kPanelColoursFirst = 240;
constexpr int kPanelColoursCount = 16;
constexpr int kSkyTimer = 0x3916;
constexpr int kSkyFadeTicks = 0x10;
constexpr int kSkyFadeSteps = 0x40;
constexpr int kSkyRefadeSteps = 0x30;

// The music: a table gives each kind of moment its track; bit 7 means the
// track waits until the next picture is shown, while the old one fades
// out.
constexpr std::size_t kMusicForMoment = 0x375C;
// The mouse pointer: the arrow.
constexpr std::size_t kArrowPointer = 0x2584;
// The pointer's other shapes: a hand over the map or the planet, and
// arrows just outside the map's view, towards each side.
constexpr std::size_t kHandPointer = 0x25C8;
constexpr std::size_t kLeftPointer = 0x26D8;
constexpr std::size_t kRightPointer = 0x2650;
constexpr std::size_t kUpPointer = 0x260C;
constexpr std::size_t kDownPointer = 0x2694;
// Where the hand shows on the globe: the planet.
constexpr std::size_t kPlanetRect = 0x2448;
// Below this row, over the panel, the pointer is an arrow.
constexpr int kPanelTop = 0x9B;
// How near the view the side arrows show.
constexpr int kSideReachX = 50;
constexpr int kSideReachY = 25;
// A zone that repeats acts again this often while the button is held.
constexpr std::uint32_t kRepeatTicks = 50;
constexpr int kSceneBottom = 152;
constexpr int kMusicFadeTicks = 0x12C;
constexpr std::uint8_t kMusicWaits = 0x80;

// The ornament's part above the panel, which the ornament covers as it
// closes, and the whole ornament.
constexpr ClipRect kOrnamentStrip{150, 137, 170, 147};
constexpr ClipRect kOrnamentArea{150, 137, 170, 160};

std::uint16_t clockTicksToday() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration<double>(now).count();
    return static_cast<std::uint16_t>(static_cast<long long>(std::fmod(seconds, 86400.0) * 1193182.0 / 65536.0));
}

class Play : public TalkHost {
public:
    Play(Game& game, int language)
        : g_(game), d_(game.display()), world_(game.resources().program()), panel_(game, language),
          talk_(game, world_, panel_, *this, language), map_(game, world_, panel_),
          saves_(game.platform().saveFolder, game.resources().program(), world_, talk_.dialogue(),
                 [this]() -> std::vector<std::uint8_t>& { return map_.mapBytes(); }),
          globe_(game, world_, panel_, map_, language, &saves_), cockpit_(game, world_, panel_, map_),
          flight_(game, world_, panel_, map_), spice_(game, world_, panel_, map_),
          orders_(game, world_, panel_, map_, talk_, spice_) {
        talk_.subtitles = subtitlesFor(language);
        talk_.enlisted = [this] { enlistFremen(); };
        talk_.ordersAsked = [this] { openOrders(); };
        orders_.finished = [this] { toggleMap(); };
        orders_.sent = [this] { stayOnMap(); };
        world_.markChanged = [this](int troop) { map_.markChanged(troop); };
        world_.markMoved = [this](int troop) { map_.markMoved(troop); };
        world_.placeChanged = [this](std::size_t record) { map_.placeChanged(record); };
        world_.random = [this](int mask) { return g_.random(mask); };
        world_.random3 = [this](int most) { return g_.random3(most); };
        world_.random2 = [this] { return g_.random2(); };
        // The troops' marks stay under the chief's box and the small map.
        map_.windows = [this] {
            return orders_.active() ? std::vector<ClipRect>{orders_.box()} : std::vector<ClipRect>{};
        };
        map_.covers = [this] {
            return spice_.shown() ? std::vector<ClipRect>{spice_.frame()} : std::vector<ClipRect>{};
        };
        g_.places().personOf = [this](int c, int& down) { return personOf(c, down); };
    }

    void run() {
        start();
        for (;;) {
            if (quit_) {
                return;
            }
            g_.waitTicks(1);
            runClock();
            if (endTalk_) {
                endTalk_ = false;
                leaveTalk();
            }
            if (map_.active()) {
                map_.tick();
            }
            if (flight_.active()) {
                flight_.tick();
                afterFlightTick();
            }
            if (hoursPassed_ > 0) {
                hoursPassed_ = 0;
                newHour();
            }
            followMouse();
        }
    }

private:
    Command commandAt(std::size_t at) const {
        const ProgramFile& program = g_.resources().program();
        return Command{program.dataU16(at), program.dataU16(at + 2)};
    }

    void start() {
        // The music stops and the screen goes black at once.
        g_.stopMusic();
        d_.clear(d_.screen());
        map_.setUp();
        if (newGame_.empty()) {
            newGame_ = saves_.contents();
        }
        // Paul starts in his room; he has come from nowhere.
        world_.goTo(kStartRoom, kStartPlace);
        world_.setU8(kCameFrom, 0);
        world_.setClock(kStartClock);
        world_.setU8(kPalaceOrnithopters, static_cast<std::uint8_t>(world_.u8(kPalaceOrnithopters) + 1));
        // The story's own lines run twice, and the dice start from the
        // time of day, counted as the PC's clock does (18.2 a second).
        talk_.runEvents();
        talk_.runEvents();
        g_.seedRandom(static_cast<std::uint16_t>(g_.fixedSeed >= 0 ? g_.fixedSeed : clockTicksToday()));

        d_.setOriginRows(0);
        d_.targetBack();
        panel_.drawAll();
        panel_.setLayout(kWalkLayout);
        panel_.drawClock(world_);
        panel_.drawSlots(world_);
        drawView();
        d_.transition(Display::kDissolveScreen);
        openOrnament();
        startWaitingMusic();
        g_.playTracksOnce = false;
        clockRunning_ = true;
        lastTick_ = g_.platform().ticks;
        showPointer(kArrowPointer);
    }

    // --- The mouse

    void showPointer(std::size_t at) {
        setPointerShape(at);
        g_.platform().cursorShown = true;
    }

    void setPointerShape(std::size_t at) {
        pointer_ = at;
        const ProgramFile& program = g_.resources().program();
        MouseCursor& c = g_.platform().cursor;
        c.hotX = static_cast<std::int16_t>(program.dataU16(at));
        c.hotY = static_cast<std::int16_t>(program.dataU16(at + 2));
        for (std::size_t row = 0; row < 16; ++row) {
            c.keep[row] = program.dataU16(at + 4 + 2 * row);
            c.bits[row] = program.dataU16(at + 36 + 2 * row);
        }
    }

    // The pointer's shape for where the mouse is.
    std::size_t pointerFor(int x, int y) const {
        ClipRect r;
        if (map_.active()) {
            r = map_.view();
        } else if (cockpit_.active()) {
            r = cockpit_.view();
        } else if (globe_.active()) {
            const ProgramFile& program = g_.resources().program();
            r = ClipRect{program.dataU16(kPlanetRect), program.dataU16(kPlanetRect + 2), program.dataU16(kPlanetRect + 4),
                         program.dataU16(kPlanetRect + 6)};
        } else {
            return kArrowPointer;
        }
        if (y >= kPanelTop) {
            return kArrowPointer;
        }
        const int left = r.left;
        const int top = r.top;
        const int right = r.right;
        const int bottom = r.bottom;
        if (x > left && x < right && y > top && y < bottom) {
            return kHandPointer;
        }
        if (y >= top && y < bottom) {
            if (left - x >= 0 && left - x < kSideReachX) {
                return kLeftPointer;
            }
            if (x - right >= 0 && x - right < kSideReachX) {
                return kRightPointer;
            }
        } else if (x >= left && x < right) {
            if (top - y >= 0 && top - y < kSideReachY) {
                return kUpPointer;
            }
            if (y - bottom >= 0 && y - bottom < kSideReachY) {
                return kDownPointer;
            }
        }
        return kArrowPointer;
    }

    void followMouse() {
        Platform& p = g_.platform();
        const int x = p.mouseX;
        const int y = p.mouseY;
        const std::size_t shape = pointerFor(x, y);
        if (shape != pointer_) {
            setPointerShape(shape);
        }
        if (globe_.pointAt(x, y)) {
            lit_ = -1;
        }
        cockpit_.pointAt(x, y);
        if (map_.active()) {
            spice_.pointAt(x, y);
        }
        lightCommand(commandUnder(x, y));
        const bool down = (p.mouseButtons & 1) != 0;
        const bool clicked = down && !buttonWasDown_;
        const bool changed = down != buttonWasDown_;
        buttonWasDown_ = down;
        if (changed && map_.active()) {
            map_.clicked();
        }
        if (!down) {
            repeatZone_ = -1;
        }
        if (clicked) {
            click(x, y);
            return;
        }
        // A zone that repeats goes on while the button stays down on it,
        // every 50 ticks from the last time it acted.
        if (down && repeatZone_ >= 0 && p.ticks - repeatSince_ >= kRepeatTicks) {
            const Zone& z = panel_.zones()[static_cast<std::size_t>(repeatZone_)];
            if ((z.flags & Zone::kActive) && z.contains(x, y)) {
                repeatSince_ = p.ticks;
                screenCommand(z.action);
            }
        }
    }

    // The line of the list of commands the mouse points at, directly or
    // through the person it names; -1 for none.
    int commandUnder(int x, int y) const {
        const auto& zones = panel_.zones();
        for (int line = 0; line < Panel::kLines; ++line) {
            const Zone& z = zones[static_cast<std::size_t>(Panel::kCommandLines + line)];
            if ((z.flags & Zone::kActive) && z.contains(x, y)) {
                return line;
            }
        }
        if (y < kSceneBottom && !map_.active() && !globe_.active() && !cockpit_.active() && !flight_.active()) {
            const int c = g_.places().characterAt(x, y);
            if (c >= 0) {
                const auto& commands = panel_.commands();
                for (std::size_t i = 0; i < commands.size() && i < static_cast<std::size_t>(Panel::kLines); ++i) {
                    if ((commands[i].phrase & Command::kPhrase) == kFirstPersonPhrase + c) {
                        return static_cast<int>(i);
                    }
                }
            }
        }
        return -1;
    }

    void lightCommand(int line) {
        if (line == lit_) {
            return;
        }
        lit_ = line;
        std::vector<Command> commands = panel_.commands();
        for (std::size_t i = 0; i < commands.size(); ++i) {
            commands[i].phrase = static_cast<std::uint16_t>(commands[i].phrase & ~Command::kLit);
            if (static_cast<int>(i) == line) {
                commands[i].phrase = static_cast<std::uint16_t>(commands[i].phrase | Command::kLit);
            }
        }
        panel_.setCommands(std::move(commands));
        d_.targetScreen();
        panel_.drawCommands();
        d_.targetBack();
        panel_.drawCommands();
    }

    void click(int x, int y) {
        const auto& zones = panel_.zones();
        const int line = commandUnder(x, y);
        if (orders_.active()) {
            // Anything but an order that can be given ends the orders;
            // while the troop is shown where to go, the small map takes the
            // clicks.
            const auto& commands = panel_.commands();
            if (line >= 0 && static_cast<std::size_t>(line) < commands.size() &&
                !(commands[static_cast<std::size_t>(line)].phrase & Command::kGrey)) {
                choose(commands[static_cast<std::size_t>(line)].action, commands[static_cast<std::size_t>(line)].phrase);
            } else if (orders_.choosing()) {
                orders_.click(x, y);
            } else {
                choose(Orders::kNoMoreOrders);
            }
            return;
        }
        if (line >= 0) {
            const auto& commands = panel_.commands();
            if (static_cast<std::size_t>(line) < commands.size()) {
                const Command c = commands[static_cast<std::size_t>(line)];
                if (!(c.phrase & Command::kGrey)) {
                    choose(c.action, c.phrase);
                }
            }
            return;
        }
        if (cockpit_.active()) {
            const Cockpit::Destination to = cockpit_.destinationAt(x, y);
            if (to.chosen) {
                flyTo(to);
            }
            return;
        }
        if (flight_.active()) {
            // Flying freely the arrows turn the ornithopter.
            for (int i = 0; i < 6; ++i) {
                const int n = Panel::kArrows + i;
                const Zone& z = zones[static_cast<std::size_t>(n)];
                if ((z.flags & Zone::kActive) && z.contains(x, y)) {
                    if (z.flags & Zone::kRepeat) {
                        repeatZone_ = n;
                        repeatSince_ = g_.platform().ticks;
                    }
                    screenCommand(z.action);
                    return;
                }
            }
            return;
        }
        if (talk_.active()) {
            return;
        }
        if (orders_.active()) {
            return; // the map stays as it is while orders are given
        }
        if (map_.active() && spice_.shown() && clickSpiceMap(x, y)) {
            return;
        }
        if (map_.active() || globe_.active()) {
            // The map's panel keeps its globe, which opens the globe.
            const Zone& globeZone = zones[0];
            if (map_.active() && (globeZone.flags & Zone::kActive) && globeZone.contains(x, y) &&
                globeZone.action == GlobeScreen::kOpen) {
                openGlobe();
                return;
            }
            for (int i = 0; i < 6; ++i) {
                const int n = Panel::kArrows + i;
                const Zone& z = zones[static_cast<std::size_t>(n)];
                if ((z.flags & Zone::kActive) && z.contains(x, y)) {
                    if (z.flags & Zone::kRepeat) {
                        repeatZone_ = n;
                        repeatSince_ = g_.platform().ticks;
                    }
                    screenCommand(z.action);
                    return;
                }
            }
            if (map_.active() && y < kSceneBottom) {
                map_.click(x, y);
            }
            if (globe_.active() && y < kSceneBottom) {
                globe_.click(x, y);
                if (globe_.leaving) {
                    backToMap();
                }
            }
            return;
        }
        for (int i = 0; i < 4; ++i) {
            const Zone& z = zones[static_cast<std::size_t>(Panel::kArrows + 1 + i)];
            if ((z.flags & Zone::kActive) && z.contains(x, y)) {
                walk(i);
                return;
            }
        }
    }

    // A command: one of the talk menu's, or talking to a person.
    void choose(std::uint16_t action, std::uint16_t phrase = 0) {
        if (orders_.command(action)) {
            return;
        }
        if (orders_.active()) {
            return; // the rest of the orders is not built yet
        }
        if (talk_.command(action)) {
            return;
        }
        if (action == MapScreen::kToggle) {
            toggleMap();
            return;
        }
        if (action == Cockpit::kOpen && !cockpit_.active()) {
            openCockpit();
            return;
        }
        if (screenCommand(action, phrase)) {
            return;
        }
        const int c = Talk::characterOf(action);
        if (c >= 0) {
            talkTo(c, phrase, false);
        }
    }

    // Starts talking to character `c` where the room shows it; or, with
    // `speakingUp`, only when it has something to say as Paul comes in.
    bool talkTo(int c, std::uint16_t phrase, bool speakingUp) {
        int x = -1;
        int y = -1;
        for (const Places::Shown& s : g_.places().shown()) {
            if (s.character == c) {
                x = s.rect.left;
                y = s.rect.top;
            }
        }
        if (!speakingUp || c == kFremen) {
            meetTroop(c, phrase);
        }
        int variant = 0;
        const int face = look(c, variant);
        if (speakingUp) {
            return talk_.speakUp(c, x, y, face, variant);
        }
        talk_.start(c, x, y, face, variant);
        return true;
    }

    // Paul comes into a room or goes for a door: the first of those
    // there, in the order of the characters, with something to say says it
    // and the talk starts. False when nobody spoke.
    bool speakUp() {
        std::vector<int> here;
        for (const World::Character& c : world_.findPresent()) {
            here.push_back(c.number);
        }
        std::sort(here.begin(), here.end());
        for (const int c : here) {
            if (c <= kChiefs && talkTo(c, 0, true)) {
                return true;
            }
        }
        return false;
    }

    // Paul is in a new room: those there may speak up. What is happening
    // stays "come in" until something else happens.
    void cameIn() {
        world_.setU8(kSituation, kComingIn);
        world_.setU8(kEntries, 1);
        speakUp();
    }

    // The picture (persons and portraits) character `c` is shown with, and
    // which of its sets; chiefs from 16 on are the further chiefs here.
    int look(int c, int& variant) const {
        variant = 0;
        std::size_t chief = chief_;
        if (c >= kChiefs) {
            chief = static_cast<std::size_t>(c - kChiefs);
            c = kChiefs;
        }
        if (c == kSmuggler) {
            variant = (world_.u8(world_.placeHere()) >> 1) + 1;
            return c;
        }
        if (c != kFremen && c != kChiefs) {
            return c;
        }
        int troop = people_.fremen;
        if (c == kChiefs) {
            if (world_.u8(kProgress) == kStoryEnd) {
                return c;
            }
            troop = chief < people_.chiefs.size() ? people_.chiefs[chief] : 0;
        }
        if (troop == 0) {
            return c;
        }
        return world_.troopFace(troop, variant);
    }

    // The persons' picture, which may also stand lower.
    int personOf(int c, int& down) const {
        down = 0;
        int variant = 0;
        const int person = look(c, variant);
        if (c == kGurney && world_.u8(kProgress) >= kGurneyLyingFrom && world_.u8(kProgress) < kGurneyLyingTo) {
            down = kLyingDown;
            return kLyingPerson;
        }
        if (c == kCaptain && (world_.u8(kCaptainFlags) & 0x10)) {
            return kCaptainHeld;
        }
        return person;
    }

    // Talking to the people of a troop: the state learns about the troop
    // first.
    void meetTroop(int c, std::uint16_t phrase) {
        if (c == kFremen && people_.fremen != 0) {
            world_.learnTroop(people_.fremen);
        } else if (c == kChiefs && !people_.chiefs.empty()) {
            int index = (phrase & Command::kPhrase) - (kFirstPersonPhrase + kChiefs);
            if (index < 0 || index >= 9) {
                index = 0;
            }
            if (index == 8) {
                index = std::max(people_.prospector - 1, 0);
            }
            chief_ = std::min<std::size_t>(static_cast<std::size_t>(index), people_.chiefs.size() - 1);
            world_.learnTroop(people_.chiefs[chief_]);
        }
    }

    // "Give orders to troop": the map around the chief's troop, with him
    // answering in a box.
    void openOrders() {
        if (people_.chiefs.empty()) {
            return;
        }
        const int troop = people_.chiefs[std::min(chief_, people_.chiefs.size() - 1)];
        world_.setU8(kMapShown, static_cast<std::uint8_t>(~world_.u8(kMapShown)));
        orders_.open(troop);
        chooseMusic();
        lit_ = -1;
    }

    // A troop was sent somewhere: the chief's box goes and the map stays
    // as it was, with its own commands.
    void stayOnMap() {
        map_.enter(map_.longitude(), map_.latitude());
        d_.copyRect(d_.back(), d_.screen(), ClipRect{0, 0, 320, 200});
        lit_ = -1;
    }

    // The Fremen work for Paul now: their troop is his, and its chief
    // answers from now on.
    void enlistFremen() {
        const int troop = people_.fremen;
        if (troop == 0) {
            return;
        }
        if (world_.enlist(troop, &map_.flatMap())) {
            talk_.raiseProgress(kAllRallied);
        }
        people_ = world_.meetPeople();
        world_.findPresent();
        if (troop == kProspectorsTroop) {
            const std::size_t t = World::troopRecord(troop);
            world_.setU8(t + 0x15, static_cast<std::uint8_t>(world_.u8(t + 0x15) + 0x18));
            world_.setU8(kKeenness, static_cast<std::uint8_t>(world_.u8(kKeenness) + 0x18));
        }
        for (std::size_t i = 0; i < people_.chiefs.size(); ++i) {
            if (people_.chiefs[i] == troop) {
                chief_ = i;
            }
        }
    }

    // A command for the map or the globe, whichever is shown.
    bool screenCommand(std::uint16_t action, std::uint16_t phrase = 0) {
        if (globe_.command(action, phrase)) {
            if (globe_.loaded) {
                globe_.loaded = false;
                afterLoading();
            }
            if (globe_.leaving) {
                backToMap();
            }
            return true;
        }
        if (over_) {
            return endCommand(action, phrase);
        }
        if (noticeShown_) {
            if (action == kGoOn) {
                endNotice();
                return true;
            }
            if (action == Flight::kChange) {
                // The cockpit opens; "Cancel" there goes on with the flight.
                talk_.endRemark();
                noticeShown_ = false;
                flight_.endNotice();
                flight_.paused = true;
                openCockpit();
                return true;
            }
            return false;
        }
        if (cockpit_.command(action)) {
            if (cockpit_.leaving) {
                leaveCockpit();
            }
            return true;
        }
        if (flight_.command(action)) {
            if (flight_.changing) {
                flight_.changing = false;
                flight_.paused = true;
                g_.closeVideo();
                openCockpit();
            } else if (flight_.arrived) {
                arrive();
            }
            return true;
        }
        if (map_.active() && action == SpiceMap::kOpen) {
            if (spice_.shown()) {
                spice_.close();
            } else {
                map_.hideBox();
                map_.hideLabel();
                spice_.occupations = false;
                spice_.open();
            }
            return true;
        }
        if (map_.active() && spice_.shown()) {
            // The arrows move the small map; its middle puts the map back
            // on Paul under it.
            int lon = 0;
            int lat = 0;
            if (map_.arrowStep(action, lon, lat)) {
                spice_.scroll(lon, lat);
                return true;
            }
            if (map_.command(action)) {
                spice_.redraw();
                return true;
            }
            return false;
        }
        return map_.command(action);
    }

    // A click while the small map of the regions is shown; false when it
    // is not on it.
    bool clickSpiceMap(int x, int y) {
        std::size_t place = 0;
        switch (spice_.click(x, y, place)) {
        case SpiceMap::Click::Outside:
            return false;
        case SpiceMap::Click::Close:
            spice_.close();
            return true;
        case SpiceMap::Click::Map:
            // A place: the map moves to it, the small map gone.
            if (place != 0) {
                spice_.forget();
                map_.centreOn(world_.u16(place + 2), static_cast<std::int16_t>(world_.u16(place + 4)));
            }
            return true;
        default:
            return true;
        }
    }

    // "TAKE AN ORNITHOPTER": the cockpit, shown at once, its sky in the
    // colours of the hour.
    void openCockpit() {
        cockpit_.enter();
        // While flying the sky keeps the colours it had when the flight
        // began.
        if (!flight_.active()) {
            setSky();
        }
        cockpit_.drawScene();
        cockpit_.show();
        lit_ = -1;
    }

    // "Cancel": the room comes back at once, or the flight goes on.
    void leaveCockpit() {
        cockpit_.leave();
        if (flight_.active()) {
            flight_.show();
            lit_ = -1;
            return;
        }
        d_.targetBack();
        g_.forgetPalette();
        drawView();
        d_.showBack();
        d_.flush();
    }

    // --- Flying

    // A destination chosen in the cockpit: its name blinks, then the
    // ornithopter takes off, or turns there when it is flying already.
    void flyTo(const Cockpit::Destination& to) {
        cockpit_.confirm();
        cockpit_.leave();
        if (to.record != 0) {
            flight_.setDestination(to.record);
        } else {
            flight_.setFree(to.longitude, to.latitude);
        }
        if (!flight_.active()) {
            takingOff_ = true;
            chooseMusic();
            takeOff();
            // The clock counted while the ornithopter took off; from here it
            // goes by the steps of the flight.
            runClock();
            const std::size_t home = world_.home();
            flight_.begin();
            takingOff_ = false;
            world_.setU8(home + kOrnithopters, static_cast<std::uint8_t>(world_.u8(home + kOrnithopters) - 1));
        }
        flight_.show();
        startWaitingMusic();
        lit_ = -1;
    }

    // The ornithopter in front of the room taking off, or landing there, a
    // step every 20 ticks with its sound; the others stay parked.
    void animateOrnithopter(int sound, int side) {
        const int count = ornithoptersHere();
        const bool sietch = (world_.room() >> 8) < 0x20;
        d_.targetBack();
        g_.forgetPalette();
        drawView(false);
        d_.showBack();
        d_.flush();
        const Surface scene = d_.back();
        const ClipRect area = g_.sceneTables().lineClip;
        g_.playSound(sound);
        // Taking off goes from standing to gone; landing comes down from
        // the step before gone.
        const int first = side > 0 ? 0 : kLastOrnithopterStep - 1;
        for (int state = first; state >= 0 && state <= kLastOrnithopterStep; state += side) {
            // The sound moves on to its next part at these steps.
            if (state == kSoundStep || (side > 0 && state == kSecondSoundStep)) {
                g_.endSoundRepeat();
            }
            d_.copyRect(scene, d_.back(), area);
            g_.places().drawTakeOff(count, sietch, state, side);
            d_.flush();
            d_.copyRect(d_.back(), d_.screen(), area);
            g_.waitTicks(kOrnithopterStepTicks);
        }
    }

    void takeOff() { animateOrnithopter(kTakeOffSound, 1); }

    // After the flight's look at the clock and the steps.
    void afterFlightTick() {
        if (flight_.hoursPassed > 0) {
            // A step moved the clock on: the count for the next part of the
            // day starts again.
            flight_.hoursPassed = 0;
            countdown_ = g_.resources().program().dataU16(kTicksPerHour);
            newHour();
        }
        if (flight_.arrived) {
            arrive();
            return;
        }
        if (flight_.caught) {
            gameOver(kCaught);
            return;
        }
        if (flight_.notice != Flight::Notice::None && !noticeShown_) {
            showNotice();
        }
    }

    // What a step came upon: a companion speaks of it in the cabin, or,
    // with nobody on board, the cockpit warns of the Harkonnens' zone.
    // The flight waits for the answer.
    void showNotice() {
        noticeShown_ = true;
        g_.closeVideo();
        d_.targetBack();
        bool what = true;
        if (flight_.noticedBy >= 0) {
            g_.selectSheet(kCabinSheet);
            g_.drawSprite(0, 0, 0);
            d_.flush();
            if (!talk_.remark(flight_.noticedBy)) {
                talk_.endRemark();
                endNotice();
                return;
            }
        } else {
            cockpit_.drawWarning();
            d_.showBack();
            d_.flush();
            what = false;
        }
        const ProgramFile& program = g_.resources().program();
        std::vector<Command> commands;
        const std::size_t list = flight_.notice == Flight::Notice::Sighting ? kSightingCommands : kZoneCommands;
        for (std::size_t at = list + 2; program.dataU16(at) != 0; at += 4) {
            Command c{program.dataU16(at), program.dataU16(at + 2)};
            if (c.action == Talk::kWhat && !what) {
                c.phrase = static_cast<std::uint16_t>(c.phrase | Command::kGrey);
            }
            commands.push_back(c);
        }
        panel_.setCommands(std::move(commands));
        d_.targetScreen();
        panel_.drawCommands();
        panel_.setArrowZones(kNoArrows);
        d_.targetBack();
        panel_.drawCommands();
        panel_.setArrowZones(kNoArrows);
        lit_ = -1;
    }

    // "GO TOWARDS THIS PLACE" or "IGNORE WARNING": the flight goes on from
    // the start of its first video.
    void endNotice() {
        const bool remarked = talk_.remarking();
        talk_.endRemark();
        noticeShown_ = false;
        flight_.endNotice();
        flight_.show();
        if (remarked) {
            openOrnament();
        }
        lit_ = -1;
    }

    // --- The end of the game

    // The sky flashes three times, then the video of Paul's death; over its
    // last picture, the face of who tells of it, in its frame, and the
    // words; the clock shows the last day.
    void gameOver(int reason) {
        over_ = true;
        overReason_ = reason;
        flashSky();
        g_.closeVideo();
        flight_.stop();
        noticeShown_ = false;
        talk_.endRemark();
        clockRunning_ = false;
        world_.setClock(kEndClock);
        flight_.playVideo(kDeathVideo);

        // The frame: filled, then its edge, then the face in it.
        const ProgramFile& program = g_.resources().program();
        d_.targetBack();
        const ClipRect frame{program.dataU16(kEndFrame), program.dataU16(kEndFrame + 2), program.dataU16(kEndFrame + 4),
                             program.dataU16(kEndFrame + 6)};
        Surface& to = d_.back();
        const int o = d_.originRows();
        const std::uint8_t edge = program.dataU8(kEndFrame + 8);
        to.fillRect(frame.left, frame.top + o, frame.right - frame.left, frame.bottom - frame.top,
                    program.dataU8(kEndFrame + 9));
        to.fillRect(frame.left, frame.top + o, frame.right - frame.left, 1, edge);
        to.fillRect(frame.left, frame.bottom - 1 + o, frame.right - frame.left, 1, edge);
        to.fillRect(frame.left, frame.top + o, 1, frame.bottom - frame.top, edge);
        to.fillRect(frame.right - 1, frame.top + o, 1, frame.bottom - frame.top, edge);
        const auto face = static_cast<std::int8_t>(program.dataU8(kEndFaces + static_cast<std::size_t>(reason)));
        if (face >= 0) {
            g_.selectSheet(resource::kFirstPortrait + face);
            const int count = static_cast<int>(g_.sheet().size());
            g_.drawSprite(static_cast<std::uint16_t>(count - 3), frame.left + 1, frame.top + 1);
            g_.drawSprite(static_cast<std::uint16_t>(count - 2), frame.left + 1, frame.top + 1);
        }
        if (!endFont_) {
            endFont_ = Font::parse(g_.resources().load(resource::kFont), Font::Size::Large);
        }
        const std::string& words = panel_.phrases().at(static_cast<std::size_t>(kFirstEndPhrase + reason - 1));
        const ClipRect box{kEndText.left, kEndText.top + o, kEndText.right, kEndText.bottom + o};
        drawTextBox(to, *endFont_, words, box, kEndTextColour);
        d_.flush();
        d_.copyRect(d_.back(), d_.screen(), ClipRect{0, 0, 320, kSceneBottom});
        panel_.drawClock(world_);
        d_.targetScreen();
        panel_.drawClock(world_);
        d_.targetBack();
        sayEnd();
        endList_ = EndList::Main;
        setEndCommands(kEndCommands);
        showPointer(kArrowPointer);
    }

    // The words are said by the one who tells of the end, the two
    // recordings in turn.
    void sayEnd() {
        const auto face = static_cast<std::int8_t>(
            g_.resources().program().dataU8(kEndFaces + static_cast<std::size_t>(overReason_)));
        if (face < 0) {
            return;
        }
        g_.talker().playVoice(std::min<int>(face, 14), kEndVoice, endTake_ ? 1 : 0);
        endTake_ = !endTake_;
    }

    void flashSky() {
        // The sky's colours as the last room had them: those of a place's
        // rooms while flying from it.
        const bool inside = world_.inside() || world_.flying();
        loadSky(skyOfTheHour(), d_.spare(), inside);
        const int first = inside ? kInsideSkyFirst : kOutsideSkyFirst;
        const int count = inside ? kInsideSkyCount : kOutsideSkyCount;
        auto step = [&](int left) {
            g_.waitTicks(kFlashStepTicks);
            d_.fadeStep(first, count, left);
        };
        for (int i = 0; i < kFlashes; ++i) {
            Palette& now = d_.palette();
            loadSky(kFlashSky, now, inside);
            for (int c = first; c < first + count; ++c) {
                d_.markChanged(c);
            }
            d_.flush();
            for (int left = 0x20; left >= 0x10; --left) {
                step(left);
            }
        }
        for (int left = 0x0F; left > 0; --left) {
            step(left);
        }
    }

    void setEndCommands(std::size_t table) {
        const ProgramFile& program = g_.resources().program();
        std::vector<Command> commands;
        for (std::size_t at = table + 2; program.dataU16(at) != 0; at += 4) {
            commands.push_back(Command{program.dataU16(at), program.dataU16(at + 2)});
        }
        panel_.setCommands(std::move(commands));
        d_.targetScreen();
        panel_.drawCommands();
        d_.targetBack();
        panel_.drawCommands();
        lit_ = -1;
    }

    bool endCommand(std::uint16_t action, std::uint16_t phrase) {
        switch (endList_) {
        case EndList::Main:
            if (action == kRestart) {
                restart();
            } else if (action == GlobeScreen::kLoad) {
                endList_ = EndList::Load;
                globe_.showLoadList();
                lit_ = -1;
            } else if (action == kLeave) {
                endList_ = EndList::Leave;
                setEndCommands(kLeaveCommands);
            } else if (action == Talk::kWhat) {
                endTake_ = !endTake_; // the same recording again
                sayEnd();
            }
            return true;
        case EndList::Leave:
            if (action == kLeaveYes) {
                quit_ = true;
            } else if (action == kLeaveNo) {
                endList_ = EndList::Main;
                setEndCommands(kEndCommands);
            }
            return true;
        case EndList::Load:
            if (action == GlobeScreen::kCancel) {
                endList_ = EndList::Main;
                setEndCommands(kEndCommands);
            } else if (action == GlobeScreen::kLoadSlot && globe_.loadSlot(phrase)) {
                resumeGame();
            }
            return true;
        }
        return true;
    }

    // "RESTART GAME": another video, then a new game.
    void restart() {
        g_.talker().halt();
        g_.stopSound();
        flight_.playVideo(kRestartVideo);
        saves_.restore(newGame_);
        over_ = false;
        start();
    }

    // A saved game was loaded: it goes on where it was.
    void resumeGame() {
        over_ = false;
        g_.talker().halt();
        g_.stopSound();
        afterLoading();
        d_.targetBack();
        g_.forgetPalette();
        panel_.drawAll();
        panel_.setLayout(kWalkLayout);
        panel_.drawClock(world_);
        panel_.drawSlots(world_);
        drawView();
        d_.transition(Display::kDissolveScreen);
        openOrnament();
        startWaitingMusic();
        clockRunning_ = true;
        lastTick_ = g_.platform().ticks;
        showPointer(kArrowPointer);
    }

    // The place is reached: the approach, or the ornithopter landing in
    // front of the place; then its first room.
    void arrive() {
        const bool flewIn = flight_.approach;
        if (!flewIn) {
            g_.closeVideo();
        }
        flight_.land();
        const int video = flight_.approachVideo();
        if (flewIn) {
            if (!flight_.playApproach()) {
                g_.closeVideo();
                if (video == Flight::kFortVideo) {
                    flight_.playVideo(video);
                } else {
                    animateOrnithopter(kLandingSound, -1);
                }
            }
        }
        d_.targetBack();
        g_.forgetPalette();
        drawView();
        d_.transition(Display::kDissolve);
        startWaitingMusic();
        lit_ = -1;
        world_.setU8(kEntries, 1);
        speakUp();
    }

    // A saved game came back: what was counting for the game before starts
    // again from the game loaded.
    void afterLoading() {
        hoursPassed_ = 0;
        lastTick_ = g_.platform().ticks;
        lit_ = -1;
    }

    // The globe in the map's panel: the globe's picture is drawn behind,
    // the map rises away from it and the planet starts turning.
    void openGlobe() {
        const std::uint16_t lon = map_.longitude();
        const int lat = map_.latitude();
        spice_.forget();
        map_.leave();
        globe_.enter(lon, lat);
        chooseMusic();
        globe_.show();
        startWaitingMusic();
        lit_ = -1;
    }

    // Back from the globe: the map's colours are sent at once, and the map
    // spirals in around the middle.
    void backToMap() {
        globe_.leave();
        g_.selectSheet(resource::kMapSprites);
        d_.flush();
        map_.enter(globe_.mapLongitude, globe_.mapLatitude);
        chooseMusic();
        d_.transition(Display::kSpiral);
        startWaitingMusic();
        g_.platform().cursorShown = true;
        lit_ = -1;
    }

    // "SEE DUNE MAP" opens the map; "EXIT MAPS" goes back to the room,
    // which dissolves in.
    void toggleMap() {
        // The state's sign of the map being shown.
        world_.setU8(kMapShown, static_cast<std::uint8_t>(-world_.u8(kMapShown)));
        if (!map_.active()) {
            map_.enter();
            chooseMusic();
            map_.show();
            lit_ = -1;
            return;
        }
        spice_.forget();
        map_.leave();
        d_.targetBack();
        panel_.setLayout(kWalkLayout);
        panel_.drawClock(world_);
        g_.forgetPalette();
        drawView();
        d_.transition(Display::kDissolveScreen);
        startWaitingMusic();
        openOrnament();
    }

    // --- TalkHost

    void showPointer(bool shown) override { g_.platform().cursorShown = shown; }
    void menuChanged() override { lit_ = -1; }

    // The ornament closes, a step every 8 ticks, over what lay behind it.
    void closeOrnament() override {
        while (panel_.ornament > 0) {
            --panel_.ornament;
            d_.copyRect(ornamentStrip_, d_.back(), kOrnamentStrip);
            d_.targetBack();
            panel_.drawOrnament();
            d_.copyRect(d_.back(), d_.screen(), kOrnamentArea);
            g_.waitTicks(kOrnamentTicks);
        }
    }

    void keepOrnamentBackground() override { d_.copyRect(d_.back(), ornamentStrip_, kOrnamentStrip); }

    void openOrnament() override {
        while (panel_.ornament < kOrnamentSteps) {
            ++panel_.ornament;
            d_.targetBack();
            panel_.drawOrnament();
            d_.targetScreen();
            panel_.drawOrnament();
            d_.targetBack();
            g_.waitTicks(kOrnamentTicks);
        }
    }

    void drawOrnament() override {
        d_.targetBack();
        panel_.drawOrnament();
    }

    // Talking ends inside a command; the room comes back once it returns.
    void backToRoom() override { endTalk_ = true; }

    void leaveTalk() {
        // Someone spoke up as Paul went for a door: he goes through it
        // unless the talk ended on a no.
        const int exit = std::exchange(walkingTo_, 0);
        const bool walking = exit > 0 && talk_.answer() == 0xFF;
        if (walking) {
            world_.goTo((world_.room() & 0xFF00) | exit, world_.place());
        }
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
        openOrnament();
        if (walking) {
            cameIn();
        }
    }

    // Through one of the room's ways out, to the next room of the place.
    void walk(int direction) {
        const auto exits = g_.places().exits();
        const auto exit = static_cast<std::int8_t>(exits[static_cast<std::size_t>(direction)]);
        if (exit <= 0) {
            return; // the ways outside are not built yet
        }
        // Those here may have something to say first, or stop him.
        world_.setU8(kSituation, kLeaving);
        world_.setU8(kGoingTo, static_cast<std::uint8_t>(exit));
        talk_.expectYes();
        const bool spoke = speakUp();
        world_.setU8(kSituation, 0);
        if (spoke) {
            walkingTo_ = exit;
            return;
        }
        world_.goTo((world_.room() & 0xFF00) | exit, world_.place());
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
        cameIn();
    }

    // --- The clock

    // The clock moves on a part of the day every so many ticks, the first
    // time on the first tick after it starts.
    void runClock() {
        const std::uint32_t now = g_.platform().ticks;
        std::uint32_t elapsed = now - lastTick_;
        lastTick_ = now;
        if (!clockRunning_ || flight_.active()) {
            return;
        }
        const int perHour = g_.resources().program().dataU16(kTicksPerHour);
        while (elapsed-- > 0) {
            if (--countdown_ < 0) {
                countdown_ = perHour;
                world_.setClock(world_.clock() + 1);
                ++hoursPassed_;
            }
        }
    }

    // A new part of the day: the clock on the panel and the sky follow.
    void newHour() {
        // The troops on their way step on, but the one given orders, and
        // the others work. The original draws the state's word 0 afresh at
        // every turn of its main loop; the steps turn it for their dice.
        world_.setU16(0, static_cast<std::uint16_t>(g_.random2()));
        world_.hourPassed(orders_.troop(), map_.flatMap());
        if (world_.mapRedraws > 0) {
            world_.mapRedraws = 0;
            map_.redraw();
        }
        d_.targetScreen();
        panel_.drawZones(1, 1);
        panel_.drawClock(world_);
        d_.copyRect(d_.screen(), d_.back(), kClockArea);
        d_.targetBack();
        fadeSky();
    }

    // --- The sky

    int skyOfTheHour() const {
        const int clock = world_.clock();
        return g_.resources().program().dataU8(kSkyOfTheHour + static_cast<std::size_t>(clock & 0x0F)) +
               ((clock >> 2) & 0x1C);
    }

    // Puts sky `n`'s colours in `to`: from inside a room the colours of
    // the windows, from outside those of the sky; then the panel's.
    void loadSky(int n, Palette& to) { loadSky(n, to, world_.inside()); }
    void loadSky(int n, Palette& to, bool inside) {
        g_.selectSheetKeepPalette(resource::kSky + (inside ? 1 : 0));
        const SpriteView v = g_.sheet().sprite(static_cast<std::size_t>(n));
        Reader r(v.data);
        r.skip(2);
        const int first = inside ? kInsideSkyFirst : kOutsideSkyFirst;
        const int count = inside ? kInsideSkyCount : kOutsideSkyCount;
        auto take = [&](int from, int n2) {
            const auto rgb = r.bytes(static_cast<std::size_t>(n2) * 3);
            for (int i = 0; i < n2; ++i) {
                const auto k = static_cast<std::size_t>(i) * 3;
                to[static_cast<std::size_t>(from + i)] = Colour{rgb[k], rgb[k + 1], rgb[k + 2]};
            }
        };
        take(first, count);
        take(kPanelColoursFirst, kPanelColoursCount);
        sky_ = n;
    }

    void markSky() {
        const bool inside = world_.inside();
        const int first = inside ? kInsideSkyFirst : kOutsideSkyFirst;
        const int count = inside ? kInsideSkyCount : kOutsideSkyCount;
        for (int i = first; i < first + count; ++i) {
            d_.markChanged(i);
        }
        for (int i = kPanelColoursFirst; i < kPanelColoursFirst + kPanelColoursCount; ++i) {
            d_.markChanged(i);
        }
    }

    // On entering a view: the sky of the hour at once, unless it is fading
    // there already.
    void setSky() {
        const int n = skyOfTheHour();
        if (skyFadeLeft_ > 0) {
            if (sky_ == n) {
                return;
            }
            skyFadeLeft_ = kSkyRefadeSteps;
            loadSky(n, d_.spare());
            return;
        }
        loadSky(n, d_.palette());
        markSky();
    }

    // When the hour turns: the colours move to the new sky a little every
    // 16 ticks.
    void fadeSky() {
        if (map_.active() || globe_.active() || flight_.active()) {
            return; // the sky is not shown
        }
        const int n = skyOfTheHour();
        if (n == sky_) {
            return;
        }
        loadSky(n, d_.spare());
        const bool running = skyFadeLeft_ > 0;
        skyFadeLeft_ = kSkyFadeSteps;
        if (!running) {
            g_.addTimer(kSkyTimer, kSkyFadeTicks, [this] { skyFadeTick(); });
        }
    }

    void skyFadeTick() {
        if (map_.active() || globe_.active() || flight_.active()) {
            skyFadeLeft_ = 0;
            g_.removeTimer(kSkyTimer);
            return;
        }
        const bool inside = world_.inside();
        d_.fadeStep(inside ? kInsideSkyFirst : kOutsideSkyFirst, inside ? kInsideSkyCount : kOutsideSkyCount,
                    skyFadeLeft_);
        d_.fadeStep(kPanelColoursFirst, kPanelColoursCount, skyFadeLeft_);
        if (--skyFadeLeft_ <= 0) {
            skyFadeLeft_ = 0;
            g_.removeTimer(kSkyTimer);
        }
    }

    // --- Music

    // What kind of moment it is, for the music; see docs/rules/game.md.
    int musicMoment() const {
        if (globe_.active()) {
            return 1;
        }
        if (map_.active()) {
            return 2;
        }
        if (flight_.active() || takingOff_) {
            return kFlyingMoment;
        }
        const int room = world_.room();
        const int kind = room >> 8;
        if (world_.inside() && (room & 0xFF) != 1) {
            if (kind < 0x20) {
                return kind < 7 ? 8 : 9;
            }
            if (kind != 0x20) {
                return 0x0C;
            }
            return (room & 0xFF) == 3 ? 0x0A : 0x0B;
        }
        return 5;
    }

    // Chooses the music for the moment: it plays at once, or the old music
    // fades out and the new waits for the next picture.
    void chooseMusic() {
        const std::uint8_t entry = g_.resources().program().dataU8(kMusicForMoment + static_cast<std::size_t>(musicMoment()));
        if (entry == 0xFF || entry == 0) {
            return;
        }
        const int track = entry & 0x3F;
        if ((entry & kMusicWaits) == 0) {
            g_.playTrack(track);
            return;
        }
        waitingTrack_ = track;
        if (track != g_.track() && g_.musicPlaying()) {
            g_.fadeMusic(kMusicFadeTicks, 0, 0x78);
        }
    }

    void startWaitingMusic() {
        if (waitingTrack_ != 0) {
            g_.playTrack(waitingTrack_);
            waitingTrack_ = 0;
        }
    }

    // The room the player is in, with the arrows and commands that go with
    // it, drawn on the back buffer.
    void drawView(bool ornithopters = true) {
        Places& places = g_.places();
        places.enter(world_.room(), world_.place());
        people_ = world_.meetPeople();
        const std::vector<World::Character> here = world_.findPresent();
        panel_.setArrows(world_, places.exits());
        panel_.drawSlots(world_);

        std::vector<Command> commands = placeCommands();
        for (const World::Character& c : here) {
            commands.push_back(Command{static_cast<std::uint16_t>(kFirstPersonPhrase + c.number), c.talk});
            if (c.number == kChiefs) {
                // A line for each chief of Paul's troops here; the
                // prospectors' has its own name once the story has come
                // that far.
                const std::size_t first = commands.size() - 1;
                for (std::size_t i = 1; i < people_.chiefs.size(); ++i) {
                    commands.push_back(Command{static_cast<std::uint16_t>(kFirstPersonPhrase + kChiefs + i), c.talk});
                }
                if (world_.u8(kProgress) >= kProspectorsNamed && people_.prospector > 0) {
                    const std::size_t at = first + static_cast<std::size_t>(people_.prospector - 1);
                    if (at < commands.size()) {
                        commands[at].phrase = kProspectorsPhrase;
                    }
                }
            }
        }
        panel_.setCommands(std::move(commands));
        lit_ = -1;
        panel_.drawCommands();

        setSky();
        chooseMusic();
        places.present = world_.present();
        places.absent = world_.away();
        places.spotTurn = world_.u8(kSpotTurn);
        places.moreChiefs = people_.chiefs.empty() ? 0 : static_cast<int>(people_.chiefs.size()) - 1;
        const int room = world_.room();
        if (world_.inside() && room == kSkyRoom) {
            places.drawView([&] { places.drawSky(); });
        } else if (world_.inside() && (room & 0xFF) == kEntrance) {
            places.drawView([&] { drawEntranceView(); });
            if (ornithopters) {
                drawParkedOrnithopters();
            }
            setSky();
        } else {
            places.drawView();
        }
        panel_.drawOrnament();
    }

    // A place's first room looks outside, with the ornithopters kept there.
    void drawEntranceView() {
        const std::size_t record = World::placeRecord(world_.place() >> 8);
        g_.places().drawOutside(world_.u8(record + 8), world_.u8(record));
    }

    void drawParkedOrnithopters() { g_.places().drawOrnithopters(ornithoptersHere(), (world_.room() >> 8) < 0x20); }

    // The commands of the room the player is in, before the people in it;
    // see docs/rules/game.md.
    std::vector<Command> placeCommands() const {
        std::vector<Command> list;
        if (!world_.inside()) {
            return list; // the views outside are not built yet
        }
        auto grey = [](Command c, bool greyed) {
            if (greyed) {
                c.phrase = static_cast<std::uint16_t>(c.phrase | Command::kGrey);
            }
            return c;
        };
        const int room = world_.room() & 0xFF;
        list.push_back(commandAt(kSeeMap));
        if (room == kEntrance) {
            if (world_.u8(kAtWar) != 0) {
                list.push_back(commandAt(kMassiveAttack));
                list.push_back(commandAt(kMassiveAttack + 4));
                list.push_back(grey(commandAt(kCallWorm), world_.u8(kProgress) < kWormProgress));
            } else {
                list.push_back(grey(commandAt(kTakeOrnithopter), ornithoptersHere() < 1));
            }
        } else if ((world_.place() >> 8) == kPalace) {
            if (room == kMessageRoom && world_.u8(kMessages) != 0) {
                const int seen = world_.u8(kMessagesSeen);
                list.push_back(grey(commandAt(kNewMessages), seen == 0));
                list.push_back(grey(commandAt(kOldMessages), seen >= world_.u8(kMessages)));
            }
            if (room == kPaulsRoom) {
                list.push_back(commandAt(kLookAtMirror));
            }
        }
        list.push_back(commandAt(kMixer));
        return list;
    }

    // The ornithopters kept at the player's place (troops there take
    // theirs out of the count; none are built yet).
    int ornithoptersHere() const {
        return world_.u8(World::placeRecord(world_.place() >> 8) + kOrnithopters);
    }

    // With the player's voices in English the characters speak without
    // subtitles, as the original does with a sound card.
    Talk::Subtitles subtitlesFor(int language) const {
        if (g_.subtitles >= 0) {
            return static_cast<Talk::Subtitles>(g_.subtitles);
        }
        return language == 0 || language == 3 ? Talk::Subtitles::None : Talk::Subtitles::Below;
    }

    Game& g_;
    Display& d_;
    World world_;
    Panel panel_;
    Talk talk_;
    MapScreen map_;
    SavedGames saves_;
    GlobeScreen globe_;
    Cockpit cockpit_;
    Flight flight_;
    SpiceMap spice_;
    Orders orders_;
    World::People people_;
    std::size_t chief_ = 0; // which of the chiefs here Paul talks to
    int walkingTo_ = 0;     // the room Paul went for when someone spoke up
    bool takingOff_ = false;
    bool endTalk_ = false;
    // A companion's remark or the cockpit's warning is shown.
    bool noticeShown_ = false;
    // The game is over: why, which list of commands is shown, and which of
    // the two recordings is said next.
    bool over_ = false;
    int overReason_ = 0;
    enum class EndList { Main, Leave, Load };
    EndList endList_ = EndList::Main;
    bool endTake_ = false;
    bool quit_ = false;
    // The game as it starts, for starting again.
    std::vector<std::uint8_t> newGame_;
    std::optional<Font> endFont_;
    Surface ornamentStrip_{320, 200};
    bool clockRunning_ = false;
    std::uint32_t lastTick_ = 0;
    int countdown_ = 0;
    int hoursPassed_ = 0;
    int sky_ = -1;
    int skyFadeLeft_ = 0;
    int waitingTrack_ = 0;
    int lit_ = -1;
    bool buttonWasDown_ = false;
    std::size_t pointer_ = kArrowPointer;
    int repeatZone_ = -1;
    std::uint32_t repeatSince_ = 0;
};

} // namespace

void runGame(Game& game, int language) { Play(game, language).run(); }

} // namespace mentat
