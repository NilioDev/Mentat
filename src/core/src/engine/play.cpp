// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/play.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/cockpit.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/globe_screen.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/saved_games.hpp"
#include "mentat/engine/talk.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/engine/world.hpp"

#include <chrono>
#include <cmath>
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
constexpr int kMessageRoom = 8;
constexpr int kPaulsRoom = 9;
// The palace's ornithopters: a new game gives it one.
constexpr std::size_t kPalaceOrnithopters = 0x100 + 0x15;
constexpr std::uint16_t kFirstPersonPhrase = 0x78;

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
          globe_(game, world_, panel_, map_, language, &saves_), cockpit_(game, world_, panel_, map_) {
        talk_.subtitles = subtitlesFor(language);
    }

    void run() {
        start();
        for (;;) {
            g_.waitTicks(1);
            runClock();
            if (endTalk_) {
                endTalk_ = false;
                leaveTalk();
            }
            if (map_.active()) {
                map_.tick();
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
        world_.goTo(kStartRoom, kStartPlace);
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
        if (y < kSceneBottom && !map_.active() && !globe_.active() && !cockpit_.active()) {
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
        if (talk_.active() || cockpit_.active()) {
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
            int x = -1;
            int y = -1;
            for (const Places::Shown& s : g_.places().shown()) {
                if (s.character == c) {
                    x = s.rect.left;
                    y = s.rect.top;
                }
            }
            talk_.start(c, x, y);
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
        if (cockpit_.command(action)) {
            if (cockpit_.leaving) {
                leaveCockpit();
            }
            return true;
        }
        return map_.command(action);
    }

    // "TAKE AN ORNITHOPTER": the cockpit, shown at once, its sky in the
    // colours of the hour.
    void openCockpit() {
        cockpit_.enter();
        setSky();
        cockpit_.drawScene();
        cockpit_.show();
        lit_ = -1;
    }

    // "Cancel": the room comes back at once.
    void leaveCockpit() {
        cockpit_.leave();
        d_.targetBack();
        g_.forgetPalette();
        drawView();
        d_.showBack();
        d_.flush();
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
        if (!map_.active()) {
            map_.enter();
            chooseMusic();
            map_.show();
            lit_ = -1;
            return;
        }
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
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
        openOrnament();
    }

    // Through one of the room's ways out, to the next room of the place.
    void walk(int direction) {
        const auto exits = g_.places().exits();
        const auto exit = static_cast<std::int8_t>(exits[static_cast<std::size_t>(direction)]);
        if (exit <= 0) {
            return; // the ways outside are not built yet
        }
        world_.goTo((world_.room() & 0xFF00) | exit, world_.place());
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
    }

    // --- The clock

    // The clock moves on a part of the day every so many ticks, the first
    // time on the first tick after it starts.
    void runClock() {
        const std::uint32_t now = g_.platform().ticks;
        std::uint32_t elapsed = now - lastTick_;
        lastTick_ = now;
        if (!clockRunning_) {
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
    void loadSky(int n, Palette& to) {
        const bool inside = world_.inside();
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
        if (map_.active() || globe_.active()) {
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
        if (map_.active() || globe_.active()) {
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
    void drawView() {
        Places& places = g_.places();
        places.enter(world_.room(), world_.place());
        const std::vector<World::Character> here = world_.findPresent();
        panel_.setArrows(world_, places.exits());
        panel_.drawSlots(world_);

        std::vector<Command> commands = placeCommands();
        for (const World::Character& c : here) {
            commands.push_back(Command{static_cast<std::uint16_t>(kFirstPersonPhrase + c.number), c.talk});
        }
        panel_.setCommands(std::move(commands));
        lit_ = -1;
        panel_.drawCommands();

        setSky();
        chooseMusic();
        places.present = world_.present();
        places.absent = world_.away();
        places.drawView();
        panel_.drawOrnament();
    }

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
        const std::size_t record = 0x100 + 0x1C * static_cast<std::size_t>((world_.place() >> 8) - 1);
        return world_.u8(record + 0x15);
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
    bool endTalk_ = false;
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
