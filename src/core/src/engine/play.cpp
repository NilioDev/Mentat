// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/play.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/attack_view.hpp"
#include "mentat/engine/cockpit.hpp"
#include "mentat/engine/flight.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/globe_screen.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/message_room.hpp"
#include "mentat/engine/mixer_panel.hpp"
#include "mentat/engine/orders.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/saved_games.hpp"
#include "mentat/engine/spice_map.hpp"
#include "mentat/engine/talk.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/engine/text_box.hpp"
#include "mentat/engine/vision.hpp"
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
constexpr std::uint16_t kContactTroops = 0x86CC; // the map's "contact Fremen troops"
constexpr std::size_t kRallied = 0x28;           // how many troops work for Paul
constexpr std::size_t kReach = 0x1176;           // how far Paul reaches his troops
constexpr int kLastFremenTroop = 0x43;
constexpr std::uint16_t kFindProspectors = 0x5B1E;
constexpr std::uint16_t kTakeOrnithopterFromMap = 0x42D9;
constexpr std::size_t kGoThereList = 0x20DA;      // "go there flying an orni", "cancel"
constexpr std::uint16_t kGoThereFlying = 0x50DB;
constexpr std::size_t kRoomKind = 0x08;           // the kind of rooms Paul is in; 0xFF outside
constexpr std::size_t kRoomNumber = 0x0B;
constexpr std::size_t kNewMessages = 0x21E8;
constexpr std::size_t kOldMessages = 0x21EC;
constexpr std::size_t kLookAtMirror = 0x21F0;
// A line's window on the planet: where it shows on the screen.
constexpr ClipRect kNamedPlaceWindow{0xA8, 0x17, 0x12A, 0x61};
constexpr std::size_t kPaulsPlaceWord = 0x114E;
// The mirror: its pictures (the room, its colours over it, the frame),
// its commands, "look away", and the flag for Chani beside Paul.
constexpr std::uint16_t kLookAtMirrorAction = 0x0EA6;
constexpr std::uint16_t kLookAway = 0x0EB9;
constexpr std::size_t kMirrorCommands = 0x20C2;
constexpr int kMirrorSheet = 0x3B;
constexpr std::size_t kCouple = 0x10;
constexpr int kCoupleBit = 0x80;
constexpr std::size_t kTired = 0xF4;
constexpr std::size_t kMixer = 0x21F4;
constexpr std::size_t kSeeMap = 0x220C;
constexpr std::size_t kCallWorm = 0x2214;
constexpr std::uint16_t kCallWormAction = 0x42D1;
// The worm comes (VER.HNM, with sound 8); the first ride moves the story
// on to 0x50.
constexpr int kWormComesVideo = 0x0E;
constexpr int kWormSound = 8;
constexpr int kRiddenStep = 0x50;
constexpr std::size_t kMassiveAttack = 0x2218; // and "fight for a whole day" after it
constexpr std::size_t kWaitForEvening = 0x21E0; // and "wait for morning" after it
constexpr int kEveningFrom = 0x0B;              // the hour from which morning is waited for
// What those commands do (the original's code for them).
constexpr std::uint16_t kWaitEvening = 0x0F48;
constexpr std::uint16_t kWaitMorning = 0x0F67;
constexpr std::uint16_t kFightAllDay = 0x0FC5;
// Walking in the desert (see docs/rules/desert.md): the sun (SUN.HSQ)
// from so many steps, every 16 steps; at the last, Paul faints (the video
// DEAD3.HNM), lies there 5 hours and is carried home.
constexpr int kSunSheet = 0x22;
constexpr int kStepsToSun = 0x14;
constexpr int kStepsToFaint = 0x37;
constexpr int kFaintVideo = 0x0B;
constexpr int kHoursUnconscious = 5;
// The desert's pictures: DN20.HSQ on, or, where the land is green,
// VG01.HSQ on.
constexpr int kDesertPictures = 0x42;
constexpr int kDesertCount = 0x13;
constexpr int kGreenPictures = 0x88;
constexpr int kGreenCount = 10;
// At war: the arrows give way to these zones (the second set while byte
// 0x11CB is set); the horizon of the battle is a sprite list in the state.
constexpr std::size_t kWarArrows = 0x1D72;
constexpr std::size_t kWarArrowsSet = 0x11CB;
constexpr std::size_t kWarHorizon = 0x11DD;
constexpr int kMassiveRounds = 20;          // blasts after a massive attack
constexpr int kBlastTicks = 0x28;           // between them, or 2 more
constexpr std::uint16_t kMassiveAttackAction = 0x7317;
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
constexpr std::size_t kStoryFlags = 0x0A;   // bit 3 the water of life offered, bit 1 drunk
constexpr std::size_t kAnswer = 0x9F;       // Paul's answer to a question: 1 accept
constexpr std::size_t kCharisma = 0x29;
constexpr std::size_t kVisionsFrom = 0xD5;
constexpr int kCharismaToLive = 100;
constexpr int kDrinkingTicks = 0x258;
constexpr int kUnconsciousTicks = 1000;
constexpr int kDiedOfTheWater = 3;
constexpr std::uint8_t kAfterTheWater = 0x11;
constexpr std::uint8_t kLeaving = 1;
constexpr std::uint8_t kComingIn = 5;
constexpr std::size_t kGoingTo = 0x0C;
constexpr std::size_t kEntries = 0x1A;
constexpr std::size_t kToSend = 0xC0;         // the spice Paul agreed to send the Emperor
constexpr std::size_t kShipmentDue = 0x1158;  // 0xFFFF while that shipment waits
constexpr int kDuncan = 3;
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
constexpr int kPlaceLost = 6;
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
constexpr std::size_t kLeaveCommands = 0x20B6;
constexpr std::uint16_t kRestart = 0x0E47;
constexpr std::uint16_t kLeave = 0x0E3E;
constexpr std::uint16_t kLeaveYes = 0x003A;
constexpr std::uint16_t kLeaveNo = 0xD2E2;
// The mixer panel (see docs/rules/mixer.md): its commands, the CD style's,
// the tracks in the CD's order, the voice its test plays and how long the
// CD style leaves between tracks.
constexpr std::size_t kMixerCommands = 0x201A;
constexpr std::size_t kCdStyleCommands = 0x206A;
constexpr std::size_t kCdOrder = 0x3804;
constexpr std::size_t kCdTracks = 9;
constexpr int kMixerMusicLines = 3;
constexpr int kVoiceTestCharacter = 5;
constexpr int kVoiceTestPhrase = 4;
constexpr std::uint32_t kCdSilenceTicks = 200;
constexpr int kCdStyleBit = 1;
constexpr int kShuffleBit = 2;
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
// The chiefs' messages: from the story's step after 0x14, a while after
// the room was shown (in ticks), as a vision.
constexpr int kMessagesFrom = 0x14;
constexpr int kQuietStep = 0x14;
// The story's scenes: their steps, menus and the blinking of the menu's
// first line.
constexpr int kSceneRoom = 0x00;
constexpr int kSceneSay = 0x02;
constexpr int kSceneRedraw = 0x04;
constexpr int kSceneTalker = 0x06;
constexpr int kSceneWait = 0x08;
constexpr int kSceneNight = 0x0A;
constexpr int kSceneNewRoom = 0x12;
constexpr int kSceneTheEnd = 0x14;
constexpr int kSceneCredits = 0x16;
// The end of the story: Paul walks into the Harkonnens' throne room.
constexpr int kThroneOfTheHarkonnens = 0x3002;
constexpr std::uint8_t kStoryOver = 0xC8;
constexpr std::uint16_t kEndingScene = 0x128F;
constexpr int kEndPictures = 0x1E;
constexpr int kFadeStepTicks = 0x10;
constexpr int kSceneCloser = 0x0C;
constexpr int kSceneSpiceMap = 0x0E;
constexpr int kSceneChiefBox = 0x10;
constexpr int kFirstFremenFace = 0x0E;
// The pictures of those two steps: sprites of resource 0x27 over the room,
// and the part of the room the second one enlarges.
constexpr int kScenePictures = 0x27;
constexpr int kNightFalls = 0x0D;
constexpr int kCloserZoom = 3;
constexpr int kCloserX = 0;
constexpr int kCloserY = 0x32;
constexpr int kSceneEnd = 0xFF;
constexpr std::size_t kSceneMenu = 0x1FBA;
constexpr std::size_t kSceneTalkMenu = 0x1FAE;
constexpr int kSceneBlinkTimer = 0x176B;
constexpr int kSceneBlinkTicks = 0x64;
constexpr std::size_t kRoomByte = 0x04;
// The palace's plan (the middle of the arrows): a panel over the room's
// right part, its frame, the plan's sprites (a list in the state) and
// marks for those in each room; "Done" closes it. See
// docs/rules/palace_plan.md.
constexpr std::uint16_t kShowPlan = 0x18EE;
constexpr ClipRect kPlanArea{160, 0, 320, 116};
constexpr std::uint8_t kPlanFill = 0xF1;
constexpr ClipRect kPlanFrame{164, 4, 316, 112};
constexpr std::uint8_t kPlanFrameColour = 0xF7;
constexpr int kPlanSheet = 0x21;
constexpr std::size_t kPlanList = 0x120B;
constexpr std::size_t kPlanRooms = 0x1426; // (x, y) bytes for rooms 2 to 12
constexpr int kPlanRoomCount = 11;
constexpr std::uint16_t kPlanPerson = 2;
constexpr std::uint16_t kPlanPaul = 1;
constexpr int kPlanMostMarks = 5;
constexpr std::size_t kPlanCommands = 0x2012;
constexpr std::uint32_t kQuietStepWait = 0x3E8;
constexpr std::uint32_t kQuietStepAgain = 0x3B6;
constexpr std::uint32_t kMessageWait = 0x32;
constexpr std::uint32_t kFirstBlur = 0x96;
constexpr std::uint32_t kSecondBlurFrom = 0xFA;
constexpr std::uint32_t kSecondBlur = 0x15E;
constexpr std::uint32_t kVisionFrom = 0x1C2;
constexpr int kVisionTimer = 0x2CC7;
constexpr int kVisionTicks = 6;
constexpr int kVisionShown = 0xBB8;
constexpr int kMessageCharacters = 0x10;
constexpr int kTroopShownSietches = 3;
constexpr std::size_t kMessageAbout = 0xEA; // and 0xEB
constexpr std::size_t kLearnedPlace = 0x11CE;
constexpr std::size_t kSpiceWindowX = 0x11C1;
constexpr std::size_t kSpiceWindowY = 0x11C3;
constexpr std::size_t kFoundUnconscious = 0xE7;
constexpr std::size_t kMessageAfter = 0xE8;
constexpr std::uint8_t kMessageAfterValue = 0x0A;
constexpr std::uint16_t kFirstPlace = 0x100;
constexpr std::uint16_t kPlacesSize = 0x7AA;
constexpr int kSkyFadeTicks = 0x10;
constexpr int kSkyFadeSteps = 0x40;
constexpr int kSkyRefadeSteps = 0x30;

// The music: a table gives each kind of moment its track; bit 7 means the
// track waits until the next picture is shown, while the old one fades
// out.
constexpr std::size_t kMusicForMoment = 0x375C;
// During a scene the music is that of moment 0, or of moment 0x0A at the
// story's step 0x48.
constexpr int kSceneOwnMusicStep = 0x48;
constexpr int kSceneOwnMusic = 0x0A;
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

// The lists of commands the mixer panel shows.
enum class MixerList { Main, CdStyle, Leave };

class Play : public TalkHost {
public:
    explicit Play(Game& game)
        : g_(game), d_(game.display()), world_(game.resources().program()), panel_(game),
          talk_(game, world_, panel_, *this), map_(game, world_, panel_),
          saves_(game.platform().saveFolder, game.resources().program(), world_, talk_.dialogue(),
                 [this]() -> std::vector<std::uint8_t>& { return map_.mapBytes(); }),
          globe_(game, world_, panel_, map_, &saves_), cockpit_(game, world_, panel_, map_),
          flight_(game, world_, panel_, map_), spice_(game, world_, panel_, map_),
          orders_(game, world_, panel_, map_, talk_, spice_), vision_(game, world_, map_),
          messageRoom_(game, world_, panel_, talk_), war_(game), mixer_(game) {
        talk_.subtitles = subtitlesFor(game.language);
        talk_.enlisted = [this] { enlistFremen(); };
        talk_.ordersAsked = [this] { openOrders(); };
        talk_.waterOfLife = [this] { drinkWaterOfLife(); };
        talk_.attackLaunched = [this] { world_.launchAttack(map_.flatMap()); };
        talk_.showNamedPlace = [this] { showNamedPlace(); };
        orders_.finished = [this] { toggleMap(); };
        messageRoom_.drawRoom = [this] {
            d_.targetBack();
            drawView();
        };
        messageRoom_.backToRoom = [this] {
            // The room's commands come back; the scene on the screen stays
            // as the device left it.
            d_.targetBack();
            drawView();
            d_.copyRect(d_.back(), d_.screen(), ClipRect{0, kSceneBottom, 320, 200});
            openOrnament();
            quietSince_ = g_.platform().ticks;
        };
        messageRoom_.ended = [this](int reason) { gameOver(reason); };
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
            std::vector<ClipRect> covers;
            if (spice_.shown()) {
                covers.push_back(spice_.frame());
            }
            if (orders_.equipping()) {
                covers.push_back(orders_.placeBox());
            }
            return covers;
        };
        g_.places().personOf = [this](int c, int& down) { return personOf(c, down); };
        g_.places().stateByte = [this](std::size_t at) -> std::optional<std::uint8_t> {
            if (at < World::kSize) {
                return world_.u8(at);
            }
            return std::nullopt;
        };
    }

    ~Play() override {
        g_.places().personOf = {};
        g_.places().stateByte = {};
    }

    GameEnd run() {
        start();
        for (;;) {
            if (quit_) {
                return won_ ? GameEnd::Won : GameEnd::Left;
            }
            if (scene_ && !g_.platform().keys.empty()) {
                g_.platform().keys.clear();
                sceneNext();
            }
            pauseWar();
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
            checkMessages();
            checkStory();
            cdTick();
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
        g_.playTracksOnce = (g_.musicStyle & kCdStyleBit) != 0;
        clockRunning_ = true;
        lastTick_ = g_.platform().ticks;
        showPointer(kArrowPointer);
    }

    // --- The chiefs' messages

    // While Paul stays in a room, the first message waiting comes to him:
    // twice the room blurs as it nears, then the vision shows (at once
    // while his place is at war).
    void checkMessages() {
        if (over_ || mirror_ || mixer_.active() || talk_.active() || map_.active() || globe_.active() ||
            cockpit_.active() || flight_.active() || orders_.active() || noticeShown_ || messageRoom_.active() ||
            planShown_ || !world_.inside()) {
            return;
        }
        if (world_.u8(kProgress) <= kMessagesFrom || world_.u8(World::kMessageQueue) == 0) {
            return;
        }
        const std::uint32_t quiet = g_.platform().ticks - quietSince_;
        if (quiet < kMessageWait) {
            return;
        }
        {
            // When the one who speaks is with Paul, it tells the message
            // itself (the chiefs only about Paul's place).
            std::uint16_t message = 0;
            std::uint16_t place = 0;
            world_.firstMessage(message, place);
            const int c = message >> 8;
            if (c < 16 && (world_.present() & (1u << c)) != 0 &&
                (c != kChiefs || place == world_.u16(kPaulsPlaceWord))) {
                world_.setU8(kSituation, 0);
                tellMessage(c, message, place);
                spokeUp(c);
                return;
            }
        }
        if (world_.u8(kAtWar) == 0) {
            if (quiet < kFirstBlur) {
                blurRoom(Display::kMosaicFine);
                return;
            }
            if (quiet < kSecondBlurFrom) {
                return;
            }
            if (quiet < kSecondBlur) {
                blurRoom(Display::kMosaicShort);
                return;
            }
            if (quiet < kVisionFrom) {
                return;
            }
        }
        showMessage();
    }

    // --- The palace's plan

    void showPlan() {
        const int room = world_.room();
        if ((room >> 8) != 0x20 || (room & 0xFF) == kEntrance) {
            return;
        }
        d_.targetBack();
        Surface& to = d_.back();
        const int o = d_.originRows();
        to.fillRect(kPlanArea.left, kPlanArea.top + o, kPlanArea.right - kPlanArea.left,
                    kPlanArea.bottom - kPlanArea.top, kPlanFill);
        // Four frames, each a pixel further out and two colours darker.
        for (int k = 0; k < 4; ++k) {
            const int l = kPlanFrame.left - 1 - k;
            const int t = kPlanFrame.top - 1 - k + o;
            const int r = kPlanFrame.right + k;
            const int b = kPlanFrame.bottom + k + o;
            const auto c = static_cast<std::uint8_t>(kPlanFrameColour - 2 * k);
            to.fillRect(l, t, r - l + 1, 1, c);
            to.fillRect(l, b, r - l + 1, 1, c);
            to.fillRect(l, t, 1, b - t + 1, c);
            to.fillRect(r, t, 1, b - t + 1, c);
        }
        g_.selectSheet(kPlanSheet);
        for (std::size_t at = kPlanList; world_.u16(at) != 0xFFFF; at += 6) {
            g_.drawSprite(world_.u16(at), world_.u16(at + 2), world_.u16(at + 4));
        }
        drawPlanPeople();
        d_.transitionIn(Display::kUnroll, kPlanArea);
        planShown_ = true;
        setEndCommands(kPlanCommands);
    }

    // A mark for each one in a room of Paul's place (five at most), those
    // going with him on a second row, and Paul's own mark.
    void drawPlanPeople() {
        std::array<int, 0x24> count{};
        const int place = world_.u8(0x07);
        for (int c = 0; c < 16; ++c) {
            const std::size_t r = 0xFD8 + 0x10 * static_cast<std::size_t>(c);
            if (world_.u8(r + 3) != place) {
                continue;
            }
            const int step = world_.u8(kProgress);
            if (world_.u8(r + 0x0E) == kGurney && step >= kGurneyLyingFrom && step < kGurneyLyingTo) {
                continue;
            }
            int at = (world_.u8(r) - 1) & 0xFF;
            if (world_.u8(r + 0x0F) & 0x40) {
                at += 0x0C;
            }
            if (at < static_cast<int>(count.size())) {
                ++count[static_cast<std::size_t>(at)];
            }
        }
        const int paulsRoom = world_.room() & 0xFF;
        if (paulsRoom <= 0x0C) {
            ++count[static_cast<std::size_t>(paulsRoom + 0x17)];
        }
        const ProgramFile& program = g_.resources().program();
        const int baseX = world_.u16(kPlanList + 2);
        const int baseY = world_.u16(kPlanList + 4);
        auto marks = [&](int n, int x, int y) {
            for (int k = 0; k < std::min(n, kPlanMostMarks); ++k) {
                g_.drawSprite(kPlanPerson, x + 4 * k, y);
            }
        };
        for (int i = 0; i < kPlanRoomCount; ++i) {
            const auto room = static_cast<std::size_t>(i + 1);
            const int x = baseX + program.dataU8(kPlanRooms + 2 * static_cast<std::size_t>(i)) + 3;
            const int y = baseY + program.dataU8(kPlanRooms + 2 * static_cast<std::size_t>(i) + 1) + 2;
            marks(count[room], x, y);
            marks(count[room + 0x0C], x, y + 7);
            if (count[room + 0x18] != 0) {
                g_.drawSprite(kPlanPaul, x + 9, y + 3);
            }
        }
    }

    void closePlan() {
        planShown_ = false;
        d_.targetBack();
        drawView();
        d_.transitionIn(Display::kRollUp, kPlanArea);
        d_.copyRect(d_.back(), d_.screen(), ClipRect{0, kSceneBottom, 320, 200});
        quietSince_ = g_.platform().ticks;
    }

    // What the story changed that shows: ways between rooms opened. And at
    // step 0x14, a long quiet outside with nobody along moves it on.
    void checkStory() {
        if (scene_ || over_ || mirror_ || mixer_.active() || talk_.active() || map_.active() || globe_.active() ||
            cockpit_.active() || flight_.active() || orders_.active() || noticeShown_ || messageRoom_.active() ||
            planShown_) {
            return;
        }
        if (world_.scene != 0 && !scene_) {
            startScene(world_.scene);
            world_.scene = 0;
        }
        if (world_.roomsChanged) {
            world_.roomsChanged = false;
            if (world_.inside()) {
                panel_.setArrows(world_, g_.places().exits());
            }
        }
        if (world_.u8(kProgress) == kQuietStep && world_.u16(World::kAwayWord) == 0 &&
            world_.u8(World::kPlaceKindByte) == World::kOutside &&
            g_.platform().ticks - quietSince_ >= kQuietStepWait) {
            quietSince_ += kQuietStepAgain;
            world_.quietAtStep14();
        }
    }

    // --- The story's scenes (see docs/rules/story_scenes.md)

    // A scene begins: the first line of the menu blinks until a click or a
    // key plays its first steps.
    void startScene(std::uint16_t script) {
        scene_ = true;
        sceneAt_ = script;
        sceneRoom_ = world_.room();
        sceneTalker_ = -1;
        sceneStanding_.clear();
        sceneMenu_ = 0;
        sceneBlink_ = false;
        g_.addTimer(kSceneBlinkTimer, kSceneBlinkTicks, [this] { sceneBlink_ = !sceneBlink_; });
        chooseMusic(); // the scene's own music
    }

    std::vector<Command> sceneCommands() const {
        std::vector<Command> list;
        if (sceneMenu_ == 0) {
            return placeCommands();
        }
        const ProgramFile& program = g_.resources().program();
        for (std::size_t at = sceneMenu_ + 2;; at += 4) {
            const Command c{program.dataU16(at), program.dataU16(at + 2)};
            if (c.phrase == 0) {
                break;
            }
            list.push_back(c);
        }
        return list;
    }

    // --- The end of the story

    // Colours `first` to `first + count - 1` fade toward the spare palette
    // in `steps` steps, 16 ticks apart; a key or a click skips the waits.
    void fadeColours(int first, int count, int steps) {
        bool waiting = true;
        for (int n = steps; n > 0; --n) {
            d_.fadeStep(first, count, n);
            if (waiting) {
                waiting = g_.waitTicks(kFadeStepTicks);
            }
        }
    }

    void blackColours(Palette& palette, int first, int count) {
        for (int i = first; i < first + count; ++i) {
            palette[static_cast<std::size_t>(i)] = Colour{};
            d_.markChanged(i);
        }
    }

    // Scene step 0x14: the room fades out, Paul and Chani fade in, the
    // Fremen's bird comes down over them and "THE END" fades in. Then the
    // scene waits.
    void theEnd() {
        g_.removeTimer(kSceneBlinkTimer);
        talk_.endRemark();
        blackColours(d_.spare(), 128, 112);
        fadeColours(128, 112, 0x28);
        d_.beginPrepare();
        g_.selectSheet(kEndPictures);
        for (std::uint16_t i = 0; i < 3; ++i) {
            g_.drawSprite(i, 0, 0);
        }
        showPaulAndChani(g_);
        d_.endPrepare();
        d_.keepPalette();
        blackColours(d_.palette(), 99, 76);
        d_.flush();
        d_.copyRect(d_.back(), d_.screen(), ClipRect{0, 0, 320, kSceneBottom});
        fadeColours(99, 76, 0x30);
        g_.talker().halt();
        d_.back().fillRect(0, d_.originRows(), 320, kSceneBottom, 0x8F);
        g_.selectSheet(kEndPictures);
        g_.drawSprite(3, 52, 0);
        d_.transition(Display::kSlideDown);
        d_.waitTicks(g_.platform().ticks, 300);
        blackColours(d_.palette(), 160, 15);
        g_.drawSprite(4, 90, 64);
        d_.flush();
        d_.copyRect(d_.back(), d_.screen(), ClipRect{0, 0, 320, kSceneBottom});
        fadeColours(160, 15, 0x20);
        sceneBlink_ = false;
        g_.addTimer(kSceneBlinkTimer, kSceneBlinkTicks, [this] { sceneBlink_ = !sceneBlink_; });
    }

    // The room on the back buffer, kept as the background; enlarged when
    // `closer`; sprite 0 of the scene's pictures at (78, 33) over it, or
    // sprite 1 at (26, 4) when closer. Then shown.
    void scenePicture(bool closer) {
        d_.targetBack();
        drawView();
        if (closer) {
            // The room without the ornament is enlarged; the ornament
            // comes over it again.
            g_.places().drawView();
            d_.zoom(kCloserZoom, kCloserX, kCloserY, d_.back(), d_.saved());
            d_.copyRect(d_.saved(), d_.back(), ClipRect{0, 0, 320, kSceneBottom});
            panel_.drawOrnament();
        } else {
            d_.saveBackground();
        }
        g_.selectSheet(kScenePictures);
        const ProgramFile& program = g_.resources().program();
        const std::size_t list = closer ? 0x2298 : 0x2290;
        g_.drawSprite(program.dataU16(list), program.dataU16(list + 2), program.dataU16(list + 4));
        d_.showBack();
        d_.flush();
        lit_ = -1;
    }

    void sceneShowRoom() {
        talk_.endRemark();
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
        lit_ = -1;
    }

    void sceneMenu(std::uint16_t menu) {
        sceneMenu_ = menu;
        panel_.setCommands(sceneCommands());
        lit_ = -1;
        d_.targetScreen();
        panel_.drawCommands();
        d_.targetBack();
        panel_.drawCommands();
    }

    // The scene's next steps, up to one that waits for a click.
    void sceneNext() {
        const ProgramFile& program = g_.resources().program();
        for (;;) {
            const int op = program.codeU8(sceneAt_++);
            switch (op) {
            case kSceneRoom: {
                const int room = program.codeU8(sceneAt_++);
                const int count = program.codeU8(sceneAt_++);
                sceneStanding_.clear();
                for (int i = 0; i < count; ++i) {
                    const int c = program.codeU8(sceneAt_++);
                    sceneStanding_.push_back(c == 0xFF ? -1 : c);
                }
                world_.setU8(kRoomByte, static_cast<std::uint8_t>(room));
                sceneMenu_ = kSceneMenu;
                sceneShowRoom();
                break;
            }
            case kSceneSay: {
                const int c = program.codeU8(sceneAt_++);
                sceneMenu(kSceneTalkMenu);
                if (c != sceneTalker_ && talk_.active()) {
                    sceneShowRoom();
                    sceneMenu(kSceneTalkMenu);
                }
                sceneTalker_ = c;
                int x = -1;
                int y = -1;
                for (const Places::Shown& sh : g_.places().shown()) {
                    if (sh.character == c) {
                        x = sh.rect.left;
                        y = sh.rect.top;
                    }
                }
                int variant = 0;
                const int face = look(c, variant);
                talk_.sceneLine(c, x, y, face, variant);
                return;
            }
            case kSceneRedraw:
                sceneMenu_ = kSceneMenu;
                sceneShowRoom();
                break;
            case kSceneTalker: {
                sceneTalker_ = program.codeU8(sceneAt_++);
                sceneMenu(kSceneMenu);
                int variant = 0;
                const int face = look(sceneTalker_, variant);
                talk_.sceneFace(sceneTalker_, face, variant);
                break;
            }
            case kSceneWait:
                return;
            case kSceneNight:
                // The hours pass until night, the sky falling dark at once;
                // then a picture over the room. Waits.
                sceneMenu_ = kSceneMenu;
                talk_.endRemark();
                while ((world_.clock() & 0x0F) < kNightFalls && !over_) {
                    world_.setClock(world_.clock() + 1);
                    newHour();
                }
                while (skyFadeLeft_ > 0) {
                    skyFadeTick();
                }
                if (over_) {
                    return;
                }
                scenePicture(false);
                return;
            case kSceneNewRoom: {
                // Another room, as step 0, coming in with a dissolve; the
                // panel's slots are emptied.
                const int room = program.codeU8(sceneAt_++);
                const int count = program.codeU8(sceneAt_++);
                sceneStanding_.clear();
                for (int i = 0; i < count; ++i) {
                    const int c = program.codeU8(sceneAt_++);
                    sceneStanding_.push_back(c == 0xFF ? -1 : c);
                }
                world_.setU8(kRoomByte, static_cast<std::uint8_t>(room));
                world_.setU8(kMessageAfter, 0);
                world_.setU16(0x1152, 0xFFFF);
                sceneMenu_ = kSceneMenu;
                talk_.endRemark();
                // Paul and Chani's faces come over the room.
                d_.beginPrepare();
                drawView();
                showPaulAndChani(g_);
                d_.endPrepare();
                d_.transition(Display::kDissolve);
                lit_ = -1;
                break;
            }
            case kSceneTheEnd:
                theEnd();
                return;
            case kSceneCredits:
                // The cast; then the game is over and starts again.
                g_.removeTimer(kSceneBlinkTimer);
                g_.removeTimer(kSkyTimer);
                skyFadeLeft_ = 0;
                talk_.endRemark();
                g_.talker().halt();
                runCastList(g_, g_.language, skyOfTheHour());
                won_ = true;
                quit_ = true;
                return;
            case kSceneCloser:
                // A part of the room enlarged, with a picture over it. Waits.
                sceneMenu_ = kSceneMenu;
                talk_.endRemark();
                scenePicture(true);
                return;
            case kSceneSpiceMap:
                // The small map of the regions over the room, while the
                // chief of the troops here speaks. Waits.
                sceneMenu(kSceneTalkMenu);
                talk_.endRemark();
                g_.talker().halt();
                talk_.sceneVoice(kChiefs);
                // At the top left of the screen, over what the scene shows.
                spice_.openFor(0, 0, 0);
                return;
            case kSceneChiefBox: {
                // The map goes; the chief goes on in a box at the top, as
                // when given orders. Waits.
                if (spice_.shown()) {
                    spice_.close();
                }
                sceneMenu(kSceneTalkMenu);
                int variant = 0;
                const int troop = world_.u8(World::placeRecord(world_.place() >> 8) + 9);
                const int face = troop != 0 ? world_.troopFace(troop, variant) : kFirstFremenFace;
                talk_.sceneBox(kChiefs, face, variant, drawChiefBox(g_, chiefBoxHigh(), face));
                return;
            }
            case kSceneEnd:
                endScene();
                return;
            default:
                break; // no script holds another step
            }
        }
    }

    void endScene() {
        g_.removeTimer(kSceneBlinkTimer);
        talk_.endRemark();
        if (talk_.inBox()) {
            talk_.endBox();
        }
        if (spice_.shown()) {
            spice_.close();
        }
        scene_ = false;
        sceneStanding_.clear();
        world_.setU8(kRoomByte, static_cast<std::uint8_t>(sceneRoom_ & 0xFF));
        sceneShowRoom();
        openOrnament();
        quietSince_ = g_.platform().ticks;
    }

    void blurRoom(int transition) {
        d_.targetBack();
        drawView();
        d_.transition(transition);
    }

    void showMessage() {
        std::uint16_t message = 0;
        std::uint16_t place = 0;
        world_.firstMessage(message, place);
        if (place != 0 && static_cast<std::uint16_t>(place - kFirstPlace) >= kPlacesSize) {
            world_.setU8(World::kMessageQueue + 3, 0);
            place = 0;
        }
        const auto about = static_cast<std::uint8_t>(message & 0xFF);
        const int c = message >> 8;
        world_.setU8(kMessageAbout, about);
        if (about == 1) {
            world_.setU8(kMessageAbout + 1, 1);
        }
        if (c >= kMessageCharacters || (c == kFremen && place == 0)) {
            world_.dropMessage();
            return;
        }
        int talker = c;
        int face = c;
        int variant = 0;
        if (place != 0) {
            world_.learnPlace(place);
            world_.namePlace(place);
            if (c >= kFremen) {
                const int troop = about == kFremen ? kTroopShownSietches : world_.u8(place + 9);
                if (troop != 0) {
                    talker = kFremen;
                    face = world_.troopFace(troop, variant);
                }
            }
        }
        closeOrnament();
        panel_.setCommands({});
        d_.targetBack();
        panel_.drawCommands();
        vision_.draw(talker >= kFremen ? place : 0);
        g_.addTimer(kVisionTimer, kVisionTicks, [this] { vision_.shimmer(); });
        talk_.vision(talker, face, variant);
        world_.dropMessage();
        g_.waitTicks(kVisionShown);
        g_.removeTimer(kVisionTimer);
        vision_.stop();
        talk_.endRemark();
        world_.setU8(kMessageAbout, 0xFF);
        world_.setU8(kMessageAfter, kMessageAfterValue);
        d_.targetBack();
        drawView();
        d_.transition(Display::kMosaic);
        openOrnament();
        quietSince_ = g_.platform().ticks;
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
        if (mixer_.holding()) {
            return kHandPointer; // a control of the mixer panel is held
        }
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

    // What a gamepad's pointer can jump to: the panel's zones in use (the
    // lines of the list that hold a command) and the people in the room.
    void publishTargets() {
        std::vector<ClipRect>& targets = g_.platform().pointerTargets;
        targets.clear();
        const auto& zones = panel_.zones();
        const int lines = static_cast<int>(panel_.commands().size());
        for (std::size_t i = 0; i < zones.size(); ++i) {
            const Zone& z = zones[i];
            if ((z.flags & Zone::kActive) == 0) {
                continue;
            }
            const int line = static_cast<int>(i) - Panel::kCommandLines;
            if (line >= 0 && line < Panel::kLines && line >= lines) {
                continue;
            }
            targets.push_back(ClipRect{z.left, z.top, z.right, z.bottom});
        }
        const auto placeTarget = [&](int x, int y) { targets.push_back(ClipRect{x - 2, y - 2, x + 3, y + 3}); };
        if (map_.active() && !globe_.active()) {
            if (spice_.shown()) {
                // The small map of the regions over the map: its places.
                const ClipRect frame = spice_.frame();
                targets.erase(std::remove_if(targets.begin(), targets.end(),
                                             [&](const ClipRect& r) {
                                                 return r.left < frame.right && r.right > frame.left &&
                                                        r.top < frame.bottom && r.bottom > frame.top;
                                             }),
                              targets.end());
                for (const SmallMap::Shown& sh : spice_.smallMap().shown()) {
                    placeTarget(sh.x, sh.y);
                }
            } else {
                // The places on the map and the troops' marks.
                for (const MapScreen::Shown& sh : map_.shownPlaces()) {
                    placeTarget(sh.x, sh.y);
                }
                for (const MapScreen::Mark& m : map_.marks()) {
                    targets.push_back(m.rect);
                }
            }
        }
        if (globe_.active()) {
            const std::vector<ClipRect> places = globe_.targets();
            targets.insert(targets.end(), places.begin(), places.end());
        }
        if (cockpit_.active()) {
            for (const SmallMap::Shown& sh : cockpit_.smallMap().shown()) {
                placeTarget(sh.x, sh.y);
            }
        }
        if (mixer_.active()) {
            const std::vector<ClipRect> knobs = mixer_.targets();
            targets.insert(targets.end(), knobs.begin(), knobs.end());
            return;
        }
        if (!map_.active() && !globe_.active() && !cockpit_.active() && !flight_.active()) {
            for (const Places::Shown& sh : g_.places().shown()) {
                targets.push_back(ClipRect{sh.rect.left, sh.rect.top, sh.rect.right, std::min(sh.rect.bottom, kSceneBottom)});
            }
        }
    }

    void followMouse() {
        publishTargets();
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
        int under = scene_ ? (sceneBlink_ ? 0 : -1) : commandUnder(x, y);
        if (under < 0 && mixer_.active()) {
            under = mixerLit_; // the music's way stays lit
        }
        lightCommand(under);
        const bool down = (p.mouseButtons & 1) != 0;
        const bool clicked = down && !buttonWasDown_;
        const bool changed = down != buttonWasDown_;
        buttonWasDown_ = down;
        const int movedX = x - lastMouseX_;
        const int movedY = y - lastMouseY_;
        lastMouseX_ = x;
        lastMouseY_ = y;
        if (mixer_.active()) {
            if (!down) {
                mixer_.release();
            } else if (!clicked && (movedX != 0 || movedY != 0)) {
                mixer_.drag(x, y, movedX, movedY);
            }
        }
        if (spice_.dragging()) {
            if (!down) {
                letGoSpiceMap();
            } else if (!clicked && (movedX != 0 || movedY != 0)) {
                spice_.drag(movedX, movedY, y);
                if (!spice_.dragging()) {
                    moveSpiceMap();
                }
            }
        }
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
        if (world_.scene != 0 && !scene_ && !orders_.active() && !map_.active() && !globe_.active()) {
            // A scene the talk began takes over at the next click.
            talk_.leave();
            startScene(world_.scene);
            world_.scene = 0;
        }
        if (scene_) {
            // In a scene " WHAT ? " says the line again; any other click
            // goes on.
            const auto& commands = panel_.commands();
            if (line >= 0 && static_cast<std::size_t>(line) < commands.size() &&
                commands[static_cast<std::size_t>(line)].action == Talk::kWhat) {
                talk_.command(Talk::kWhat);
                return;
            }
            sceneNext();
            return;
        }
        if (orders_.active()) {
            // Anything but an order that can be given ends the orders;
            // while the troop is shown where to go, the small map takes the
            // clicks.
            const auto& commands = panel_.commands();
            if (line >= 0 && static_cast<std::size_t>(line) < commands.size() &&
                !(commands[static_cast<std::size_t>(line)].phrase & Command::kGrey)) {
                choose(commands[static_cast<std::size_t>(line)].action, commands[static_cast<std::size_t>(line)].phrase);
            } else if (orders_.choosing() || orders_.equipping()) {
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
                    chosenLine_ = line;
                    choose(c.action, c.phrase);
                }
            }
            return;
        }
        if (mirror_) {
            return;
        }
        if (mixer_.active()) {
            clickMixer(x, y);
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
                placeMenu();
            }
            if (globe_.active() && y < kSceneBottom) {
                globe_.click(x, y);
                if (globe_.leaving) {
                    backToMap();
                }
            }
            return;
        }
        if (messageRoom_.active()) {
            return;
        }
        if (planShown_) {
            closePlan(); // a click anywhere closes the plan
            return;
        }
        const Zone& middle = zones[static_cast<std::size_t>(Panel::kArrows + 5)];
        if ((middle.flags & Zone::kActive) && middle.contains(x, y) && middle.action == kShowPlan) {
            showPlan();
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
        if (mirror_) {
            endCommand(action, phrase);
            return;
        }
        if (mixer_.active()) {
            mixerCommand(action);
            return;
        }
        if (action == MixerPanel::kOpen && !map_.active() && !talk_.active()) {
            openMixer();
            return;
        }
        if (action == kLookAtMirrorAction && world_.inside() && !map_.active() && !talk_.active()) {
            lookAtMirror();
            return;
        }
        if (orders_.command(action)) {
            return;
        }
        if (orders_.active()) {
            return; // nothing else acts while the orders are shown
        }
        if (talk_.command(action)) {
            return;
        }
        if (planShown_) {
            closePlan(); // "Done", or any other command
            return;
        }
        if (messageRoom_.command(action, chosenLine_)) {
            return;
        }
        if (messageRoom_.active() && talk_.active()) {
            return; // only "Viewed" ends the news
        }
        if (action == MapScreen::kToggle) {
            toggleMap();
            return;
        }
        if (action == kTakeOrnithopterFromMap && map_.active() && !orders_.active()) {
            // To the place's first room, and away from there.
            world_.goTo((world_.room() & 0xFF00) | 1, world_.place());
            toggleMap();
            openCockpit();
            return;
        }
        if (action == kCallWormAction && !map_.active() && !flight_.active() && !cockpit_.active() &&
            world_.u8(kProgress) >= kWormProgress) {
            callWorm();
            return;
        }
        if (action == kMassiveAttackAction && atWar() && !map_.active()) {
            massiveAttack();
            return;
        }
        if ((action == kWaitEvening || action == kWaitMorning || action == kFightAllDay) && !map_.active()) {
            waitFor(action);
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

    enum class Opening { Asked, SpeakingUp, Message };

    // Starts talking to character `c` where the room shows it; or, with
    // `speakingUp`, only when it has something to say as Paul comes in.
    bool talkTo(int c, std::uint16_t phrase, bool speakingUp) {
        return talkTo(c, phrase, speakingUp ? Opening::SpeakingUp : Opening::Asked);
    }

    bool talkTo(int c, std::uint16_t phrase, Opening opening) {
        const bool speakingUp = opening != Opening::Asked;
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
        if (opening == Opening::Message) {
            talk_.sayMessage(c, x, y, face, variant);
            return true;
        }
        if (speakingUp) {
            return talk_.speakUp(c, x, y, face, variant);
        }
        talk_.start(c, x, y, face, variant);
        return true;
    }

    // A companion in the room tells its message itself (see
    // docs/rules/messages.md): the talk starts with it.
    void tellMessage(int c, std::uint16_t message, std::uint16_t place) {
        const std::uint16_t learned = world_.u16(kLearnedPlace);
        if (place != 0) {
            world_.learnPlace(place);
        }
        world_.setU8(kMessageAbout, static_cast<std::uint8_t>(message & 0xFF));
        talkTo(c, 0, Opening::Message);
        world_.setU8(kMessageAbout, 0xFF);
        world_.learnPlace(learned);
    }

    // Whoever spoke up has no more messages to tell.
    void spokeUp(int c) {
        world_.dropMessagesFrom(c);
        world_.setU8(kFoundUnconscious, 0);
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
            if (c > kChiefs) {
                continue;
            }
            if (talkTo(c, 0, true)) {
                spokeUp(c);
                return true;
            }
            std::uint16_t message = 0;
            std::uint16_t place = 0;
            if (world_.messageFrom(c, message, place)) {
                tellMessage(c, message, place);
                spokeUp(c);
                return true;
            }
        }
        return false;
    }

    // Paul is in a new room: those there may speak up. What is happening
    // stays "come in" until something else happens.
    void cameIn() {
        quietSince_ = g_.platform().ticks;
        world_.setU8(kSituation, kComingIn);
        roomEntered();
    }

    // What coming into a room brings, as the original does it on walking
    // in and after the water of life.
    void roomEntered() {
        world_.setU8(kEntries, 1);
        shipSpice();
        speakUp();
    }

    // Coming into the message room with Duncan while a shipment for the
    // Emperor is agreed: it goes (see docs/rules/shipments.md).
    void shipSpice() {
        if (world_.u8(kRoomByte) != kMessageRoom || !(world_.present() & (1u << kDuncan))) {
            return;
        }
        const unsigned amount = world_.u16(kToSend) & world_.u16(kShipmentDue);
        if (amount == 0) {
            return;
        }
        world_.sendShipment(amount);
        panel_.drawSlots(world_);
        messageRoom_.shipmentLeaves();
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
    // Stilgar's water of life (see docs/rules/stilgar.md): offered once;
    // when Paul accepted, he drinks it once the line is said, and lives
    // only with charisma enough.
    void drinkWaterOfLife() {
        world_.setU8(kStoryFlags, static_cast<std::uint8_t>(world_.u8(kStoryFlags) | 0x08));
        if (world_.u8(kAnswer) != 1) {
            return;
        }
        Talker& talker = g_.talker();
        if (talker.speaking()) {
            while (talker.speaking()) {
                g_.waitTicks(1);
            }
        } else {
            g_.waitTicks(kDrinkingTicks);
        }
        if (world_.u8(kCharisma) < kCharismaToLive) {
            talk_.leave();
            gameOver(kDiedOfTheWater);
            return;
        }
        world_.setU8(kStoryFlags, static_cast<std::uint8_t>(world_.u8(kStoryFlags) | 0x02));
        world_.setU8(kVisionsFrom, 0xFF);
        d_.transition(Display::kFadeToBlack);
        d_.waitTicks(g_.platform().ticks, kUnconsciousTicks);
        talk_.leave();
        d_.targetBack();
        drawView();
        d_.transition(Display::kFadeInFromBlack);
        lit_ = -1;
        for (int hour = 0; hour < 3; ++hour) {
            world_.setClock(world_.clock() + 1);
            newHour();
            if (over_) {
                return;
            }
        }
        quietSince_ = g_.platform().ticks;
        world_.setU8(kSituation, kAfterTheWater);
        roomEntered();
    }

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
        talk_.harkonnens = people_.harkonnens;
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
        if (map_.active() && goingTo_ != 0) {
            if (action == kGoThereFlying) {
                goThereFlying();
                return true;
            }
            if (action == Orders::kCancel) {
                map_.hideLabel();
                leavePlaceMenu();
                return true;
            }
        }
        if (map_.active() && action == kFindProspectors) {
            findProspectors();
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
        if (map_.active() && action == kContactTroops) {
            contactTroops();
            return true;
        }
        const bool done = map_.command(action);
        if (goingTo_ != 0 && map_.labelled() == 0) {
            leavePlaceMenu(); // the label went with the view
        }
        return done;
    }

    // "Find prospectors": the map moves to the prospectors' troop, whose
    // orders open when Paul can reach it and the small map is not open.
    void findProspectors() {
        constexpr int kProspectors = 3;
        orders_.setLastTroop(kProspectors);
        const std::size_t t = World::troopRecord(kProspectors);
        map_.hideBox();
        map_.hideLabel();
        map_.centreOn(world_.u16(t + 6), static_cast<std::int16_t>(world_.u16(t + 8)));
        if (spice_.shown()) {
            spice_.redraw();
            return;
        }
        const bool here = !(world_.u8(t + 3) & 0x40) && world_.inside() &&
                          world_.u16(t + 4) == World::placeRecord(world_.place() >> 8);
        if (world_.u16(kReach) > 1 || here) {
            contactTroops();
        }
    }

    // A place's label on the map: from a sietch, or near the entrance
    // elsewhere, Paul may fly there; the map's commands come back when the
    // label goes.
    void placeMenu() {
        const std::size_t place = map_.labelled();
        const std::size_t here = world_.inside() ? World::placeRecord(world_.place() >> 8) : 0;
        if (place != 0 && place != here && !orders_.active() && !spice_.shown()) {
            const int kind = world_.u8(kRoomKind);
            if (kind != 0xFF && here != 0 && (world_.u8(kRoomNumber) <= 2 || kind < 0x20)) {
                std::vector<Command> list{commandAt(kGoThereList + 2), commandAt(kGoThereList + 6)};
                if (!list.empty() && world_.freeEquipment(here)[1] == 0) {
                    list[0].phrase = static_cast<std::uint16_t>(list[0].phrase | Command::kGrey);
                }
                goingTo_ = place;
                showCommands(std::move(list));
                return;
            }
        }
        leavePlaceMenu();
    }

    void leavePlaceMenu() {
        if (goingTo_ == 0) {
            return;
        }
        goingTo_ = 0;
        showCommands(map_.commands());
    }

    void showCommands(std::vector<Command> list) {
        panel_.setCommands(std::move(list));
        d_.targetScreen();
        panel_.drawCommands();
        d_.targetBack();
        panel_.drawCommands();
        lit_ = -1;
    }

    // "Go there flying an orni": from the place's first room, away to the
    // place of the label.
    void goThereFlying() {
        const std::size_t to = goingTo_;
        goingTo_ = 0;
        map_.hideLabel();
        world_.goTo((world_.room() & 0xFF00) | 1, world_.place());
        toggleMap();
        Cockpit::Destination d;
        d.chosen = true;
        d.record = to;
        launch(d);
    }

    // "Contact Fremen troops" on the map: the orders of the troop at Paul's
    // place while he reaches no further; otherwise of the troop last given
    // orders, or the next one the map marks.
    void contactTroops() {
        if (world_.u8(kRallied) == 0) {
            return;
        }
        int troop = 0;
        if (world_.u16(kReach) < 2) {
            if (world_.inside()) {
                troop = world_.u8(World::placeRecord(world_.place() >> 8) + 9);
            }
        } else {
            const int last = orders_.lastTroop();
            bool marked = false;
            for (const MapScreen::Mark& m : map_.marks()) {
                marked = marked || (last != 0 && m.troop == last);
            }
            troop = marked ? last : orders_.nextMarkedTroop(0);
        }
        if (troop == 0 || troop > kLastFremenTroop) {
            return;
        }
        map_.hideBox();
        map_.hideLabel();
        orders_.open(troop, false);
        chooseMusic();
        lit_ = -1;
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
        case SpiceMap::Click::Title:
            spice_.startDrag();
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

    // The small map of the regions let go after a drag: when it moved,
    // the map comes back under where it was and it is drawn at its new
    // place.
    void letGoSpiceMap() {
        if (spice_.endDrag()) {
            moveSpiceMap();
        }
    }

    void moveSpiceMap() {
        const ClipRect old = spice_.frame();
        if (old.left == static_cast<std::int16_t>(world_.u16(kSpiceWindowX)) &&
            old.top == static_cast<std::int16_t>(world_.u16(kSpiceWindowY))) {
            return;
        }
        d_.copyRect(d_.back(), d_.screen(), old);
        spice_.moveToKept();
    }

    // "TAKE AN ORNITHOPTER": the cockpit, shown at once, its sky in the
    // colours of the hour.
    void openCockpit() {
        if (!flight_.active()) {
            cockpit_.worm = false;
            flight_.worm = false;
        }
        if (cockpit_.worm) {
            // On the way, the worm's map comes over the ride.
            cockpit_.enter();
            cockpit_.drawScene();
            cockpit_.show();
            lit_ = -1;
            return;
        }
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

    // "CALL A WORM": where to, on a map over the view.
    void callWorm() {
        cockpit_.worm = true;
        flight_.worm = true;
        d_.targetBack();
        g_.forgetPalette();
        drawView();
        cockpit_.enter();
        cockpit_.drawScene();
        cockpit_.show();
        lit_ = -1;
    }

    // The worm comes out of the sand and Paul climbs on.
    void wormComes() {
        world_.raiseStory(kRiddenStep);
        g_.playSound(kWormSound);
        flight_.playVideo(kWormComesVideo);
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
        launch(to);
    }

    // The ornithopter flies to `to`: it takes off first unless flying.
    void launch(const Cockpit::Destination& to) {
        if (to.record != 0) {
            flight_.setDestination(to.record);
        } else {
            flight_.setFree(to.longitude, to.latitude);
        }
        if (!flight_.active() && flight_.worm) {
            takingOff_ = true;
            chooseMusic();
            wormComes();
            runClock();
            flight_.begin();
            takingOff_ = false;
        } else if (!flight_.active()) {
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
        war_.stop();
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
            endFont_ = Font::parse(g_.resources().load(g_.fontFile()), Font::Size::Large);
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

    // The list the end of the game or the mirror goes back to.
    std::size_t mainEndCommands() const { return mirror_ ? kMirrorCommands : kEndCommands; }

    bool endCommand(std::uint16_t action, std::uint16_t phrase) {
        switch (endList_) {
        case EndList::Main:
            if (action == kRestart) {
                if (mirror_) {
                    leaveMirrorNow();
                }
                restart();
            } else if (action == kLookAway && mirror_) {
                leaveMirror();
            } else if (action == GlobeScreen::kSave && mirror_) {
                endList_ = EndList::Save;
                globe_.showSaveList();
                lit_ = -1;
            } else if (action == GlobeScreen::kLoad) {
                endList_ = EndList::Load;
                globe_.showLoadList();
                lit_ = -1;
            } else if (action == kLeave) {
                endList_ = EndList::Leave;
                setEndCommands(kLeaveCommands);
            } else if (action == Talk::kWhat && !mirror_) {
                endTake_ = !endTake_; // the same recording again
                sayEnd();
            }
            return true;
        case EndList::Leave:
            if (action == kLeaveYes) {
                quit_ = true;
            } else if (action == kLeaveNo) {
                endList_ = EndList::Main;
                setEndCommands(mainEndCommands());
            }
            return true;
        case EndList::Load:
            if (action == GlobeScreen::kCancel) {
                endList_ = EndList::Main;
                setEndCommands(mainEndCommands());
            } else if (action == GlobeScreen::kLoadSlot && globe_.loadSlot(phrase)) {
                if (mirror_) {
                    leaveMirrorNow();
                }
                resumeGame();
            }
            return true;
        case EndList::Save:
            if (action == GlobeScreen::kCancel ||
                (action == GlobeScreen::kSaveSlot && globe_.saveSlot(phrase))) {
                endList_ = EndList::Main;
                setEndCommands(mainEndCommands());
            }
            return true;
        }
        return true;
    }

    // A line shows the place it speaks of in a window on the planet, as a
    // vision does, unless that is Paul's own place or the map is shown.
    void showNamedPlace() {
        const std::size_t place = world_.namedPlace;
        if (place == 0 || map_.active() || place == world_.u16(kPaulsPlaceWord)) {
            return;
        }
        vision_.drawWindow(place);
        d_.copyRect(d_.back(), d_.screen(), kNamedPlaceWindow);
    }

    // --- The mixer panel (see docs/rules/mixer.md)

    void openMixer() {
        talk_.endRemark();
        beforeMixer_ = panel_.commands();
        mixer_.open(static_cast<int>(talk_.subtitles));
        showMixerList(MixerList::Main);
    }

    void showMixerList(MixerList list) {
        mixerList_ = list;
        mixerLit_ = -1;
        switch (list) {
        case MixerList::Main: {
            setEndCommands(kMixerCommands);
            std::vector<Command> commands = panel_.commands();
            if (!g_.hasMusic()) {
                for (int i = 0; i < kMixerMusicLines && i < static_cast<int>(commands.size()); ++i) {
                    commands[static_cast<std::size_t>(i)].phrase |= Command::kGrey;
                }
                showCommands(std::move(commands));
            } else {
                mixerLit_ = g_.musicOff ? 0 : (g_.musicStyle & kCdStyleBit) + 1;
            }
            break;
        }
        case MixerList::CdStyle:
            setEndCommands(kCdStyleCommands);
            mixerLit_ = (g_.musicStyle & kShuffleBit) != 0 ? 1 : 0;
            break;
        case MixerList::Leave:
            setEndCommands(kLeaveCommands);
            break;
        }
    }

    // "Done": the scene under the window comes back with the commands the
    // panel replaced, and lines are shown as chosen.
    void closeMixer() {
        talk_.subtitles = static_cast<Talk::Subtitles>(mixer_.subtitles());
        mixer_.close();
        mixerLit_ = -1;
        showCommands(beforeMixer_);
    }

    void clickMixer(int x, int y) {
        switch (mixer_.press(x, y)) {
        case MixerPanel::Click::Outside:
            // As "Done", or back from a list the panel opened.
            if (mixerList_ == MixerList::Main) {
                closeMixer();
            } else {
                showMixerList(MixerList::Main);
            }
            break;
        case MixerPanel::Click::VoiceTest:
            if (g_.talker().playVoice(kVoiceTestCharacter, kVoiceTestPhrase)) {
                g_.waitSound();
            }
            break;
        case MixerPanel::Click::Language:
            // The words of the commands are read again in the new language.
            panel_.languageChanged();
            showMixerList(MixerList::Main);
            break;
        case MixerPanel::Click::Nothing:
            break;
        }
    }

    void mixerCommand(std::uint16_t action) {
        switch (mixerList_) {
        case MixerList::Main:
            if (action == MixerPanel::kMusicOff) {
                g_.musicOff = true;
                closeMixer();
                g_.stopMusic();
            } else if (action == MixerPanel::kGameRelative) {
                g_.musicOff = false;
                g_.musicStyle = 0;
                closeMixer();
                g_.playTracksOnce = false;
                g_.stopMusic();
                chooseMusic();
                startWaitingMusic();
            } else if (action == MixerPanel::kCdStyle) {
                showMixerList(MixerList::CdStyle);
            } else if (action == kLeave) {
                showMixerList(MixerList::Leave);
            } else if (action == MixerPanel::kDone) {
                closeMixer();
            }
            break;
        case MixerList::CdStyle:
            if (action == MixerPanel::kStandardOrder) {
                g_.musicStyle = kCdStyleBit;
                readCdOrder();
                startCdStyle();
            } else if (action == MixerPanel::kShuffle) {
                g_.musicStyle = kCdStyleBit | kShuffleBit;
                startCdStyle();
            } else if (action == MixerPanel::kCancelCdStyle) {
                closeMixer();
            }
            break;
        case MixerList::Leave:
            if (action == kLeaveYes) {
                quit_ = true;
            } else if (action == kLeaveNo) {
                showMixerList(MixerList::Main);
            }
            break;
        }
    }

    void readCdOrder() {
        const ProgramFile& program = g_.resources().program();
        for (std::size_t i = 0; i < kCdTracks; ++i) {
            cdOrder_[i] = program.dataU8(kCdOrder + i);
        }
    }

    // The CD style starts from the first track of its order, shuffled
    // first for the shuffle.
    void startCdStyle() {
        g_.stopMusic();
        closeMixer();
        g_.musicOff = false;
        g_.playTracksOnce = true;
        if ((g_.musicStyle & kShuffleBit) != 0) {
            shuffleCdOrder();
        }
        cdNext_ = 0;
        playNextCdTrack();
    }

    // Eighteen times two places of the order, drawn at random, swap.
    void shuffleCdOrder() {
        constexpr int kSwaps = 18;
        for (int i = 0; i < kSwaps; ++i) {
            const auto a = static_cast<std::size_t>(g_.random3(static_cast<int>(kCdTracks) - 1));
            const auto b = static_cast<std::size_t>(g_.random3(static_cast<int>(kCdTracks) - 1));
            std::swap(cdOrder_[a], cdOrder_[b]);
        }
    }

    void playNextCdTrack() {
        if (cdOrder_[0] == 0) {
            readCdOrder();
        }
        if (cdNext_ >= kCdTracks) {
            // The end of the order: again from the start, shuffled anew.
            cdNext_ = 0;
            if ((g_.musicStyle & kShuffleBit) != 0) {
                shuffleCdOrder();
            }
        }
        const int track = cdOrder_[cdNext_++];
        g_.stopMusic();
        g_.playTracksOnce = true;
        g_.playTrack(track);
        cdSilent_ = false;
    }

    // In the CD style a track that has ended is followed by the next one
    // after a short silence, unless a voice speaks or the place is at war.
    void cdTick() {
        if ((g_.musicStyle & kCdStyleBit) == 0 && waitingTrack_ != 0 && !g_.musicPlaying()) {
            // The music waiting for the last one to fade starts once it has.
            startWaitingMusic();
            return;
        }
        if ((g_.musicStyle & kCdStyleBit) == 0 || g_.musicOff || !g_.hasMusic() || g_.musicPlaying()) {
            cdSilent_ = false;
            return;
        }
        if (atWar() || g_.platform().soundQueued[static_cast<int>(SoundChannel::Digital)] > 0) {
            return;
        }
        const std::uint32_t now = g_.platform().ticks;
        if (!cdSilent_) {
            cdSilent_ = true;
            cdSilentSince_ = now;
            return;
        }
        if (now - cdSilentSince_ >= kCdSilenceTicks) {
            playNextCdTrack();
        }
    }

    // --- The mirror

    // "LOOK AT MIRROR": Paul's face in the mirror of his room, Chani's
    // beside it once she is with him; the clock stops meanwhile, and the
    // commands are those of the game itself. See docs/rules/mirror.md.
    void lookAtMirror() {
        mirror_ = true;
        clockRunning_ = false;
        talk_.endRemark();
        closeOrnament();
        d_.beginPrepare();
        d_.targetBack();
        g_.selectSheet(kMirrorSheet);
        g_.drawSprite(0, 0, 0);
        g_.drawSprite(1, 0, 0);
        const int face = paulsFace(world_.clock(), world_.u8(kTired));
        if ((world_.u8(kCouple) & kCoupleBit) != 0) {
            showPaulAndChani(g_, face);
        } else {
            showPaul(g_, face);
        }
        g_.selectSheet(kMirrorSheet);
        g_.drawSprite(2, 0, 0);
        panel_.setArrowZones(kNoArrows);
        d_.endPrepare();
        d_.transition(Display::kSquash);
        endList_ = EndList::Main;
        setEndCommands(kMirrorCommands);
        showPointer(kArrowPointer);
    }

    // The mirror is left for something else: its faces go and the clock
    // runs again.
    void leaveMirrorNow() {
        mirror_ = false;
        g_.talker().stop();
        clockRunning_ = true;
        lastTick_ = g_.platform().ticks;
    }

    // "Look away from the mirror": the room comes back the same way.
    void leaveMirror() {
        leaveMirrorNow();
        d_.targetBack();
        drawView();
        d_.transition(Display::kSquash);
        openOrnament();
        lit_ = -1;
        quietSince_ = g_.platform().ticks;
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
        // A worm leaves Paul at the place without more ado.
        const bool flewIn = flight_.approach && !flight_.worm;
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
            messageRoom_.leave();
            map_.enter();
            chooseMusic();
            map_.show();
            lit_ = -1;
            return;
        }
        spice_.forget();
        goingTo_ = 0;
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
            if (endingBegins()) {
                return;
            }
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
        if (!world_.inside()) {
            walkDesert(direction + 1);
            return;
        }
        const auto exits = g_.places().exits();
        const auto exit = static_cast<std::int8_t>(exits[static_cast<std::size_t>(direction)]);
        if (exit == 0) {
            return;
        }
        if (exit < 0) {
            walkDesert(-exit); // a way outside
            return;
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
        if (endingBegins()) {
            return;
        }
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
        cameIn();
    }

    // A step in the desert, or out into it (see docs/rules/desert.md).
    void walkDesert(int way) {
        const bool outside = !world_.inside();
        const bool arrived = world_.walkDesert(way);
        d_.targetBack();
        g_.forgetPalette();
        drawView();
        d_.showBack();
        d_.flush();
        if (arrived) {
            quietSince_ = g_.platform().ticks;
            world_.setU8(kEntries, 1);
            speakUp();
            return;
        }
        if (outside) {
            sunBeatsDown();
        }
    }

    // Long in the desert, every 16 steps the sun dazzles Paul a little
    // longer, until he faints.
    void sunBeatsDown() {
        const int over = world_.desertSteps() - kStepsToSun;
        if (over < 0 || (over & 0x0F) != 0) {
            return;
        }
        d_.targetBack();
        g_.selectSheet(kSunSheet);
        g_.drawSprite(0, 0, 0);
        d_.showBack();
        d_.flush();
        g_.waitTicks((over >> 4) + 1);
        if (world_.desertSteps() >= kStepsToFaint) {
            faint();
            return;
        }
        d_.targetBack();
        g_.forgetPalette();
        drawView();
        d_.showBack();
        d_.flush();
    }

    // Paul faints in the sand; hours later he wakes where he last came in,
    // and those there tell him how he was found.
    void faint() {
        flight_.playVideo(kFaintVideo);
        if (!passHours(kHoursUnconscious)) {
            return;
        }
        world_.carriedHome();
        settleSky();
        d_.targetBack();
        g_.forgetPalette();
        drawView();
        d_.transition(Display::kDissolve);
        lit_ = -1;
        cameIn();
    }

    // Hours go by at once, as they would have; false when the game ended
    // meanwhile.
    bool passHours(int hours) {
        passingHours_ = true;
        for (int i = 0; i < hours; ++i) {
            world_.setClock(world_.clock() + 1);
            newHour();
            if (over_) {
                passingHours_ = false;
                return false;
            }
        }
        passingHours_ = false;
        if (!atWar()) {
            war_.stop();
        }
        return true;
    }

    // "Wait for evening" (until the 12th part of the day) and "wait for
    // morning" (the next day's first); "fight for a whole day" passes the
    // hours while the fight goes on, 16 at most.
    void waitFor(std::uint16_t action) {
        const int clock = world_.clock();
        int hours = 0;
        if (action == kFightAllDay) {
            for (int i = 0; i < 16; ++i) {
                if (!passHours(1)) {
                    return;
                }
                if (world_.u8(kAtWar) == 0) {
                    break;
                }
            }
        } else {
            if (action == kWaitEvening) {
                if ((clock & 0x0F) >= 0x0C) {
                    return;
                }
                hours = ((clock & 0xFFF0) | 0x0C) - clock;
            } else {
                if ((clock & 0x0F) < kEveningFrom) {
                    return;
                }
                hours = ((clock & 0xFFF0) + 0x10) - clock;
            }
            if (world_.u8(kProgress) == kQuietStep) {
                quietSince_ -= kQuietStepWait; // waiting counts as a long quiet
            }
            if (!passHours(hours)) {
                return;
            }
        }
        settleSky();
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
    }

    // The picture of the desert where Paul stands: the place he last came
    // into while he is still on its way out, else one of the desert's (or
    // of the green land's) by where he is.
    int desertPicture() {
        const std::size_t home = world_.home();
        const int room = world_.room();
        const int place = world_.place();
        if (room == world_.u16(home + 2)) {
            const int p = g_.places().outsidePicture(world_.u8(home + 8), world_.u8(home), (place >> 8) & 0xFF);
            if (p >= 0) {
                return p;
            }
        }
        bool green = (world_.u8(home + 0x0A) & 1) != 0;
        const FlatMap& map = map_.flatMap();
        const int point = map.pointAt(static_cast<std::uint16_t>(room), static_cast<std::int8_t>(place & 0xFF));
        const auto& bytes = map.bytes();
        for (int i = point - 1; i <= point + 2 && !green; ++i) {
            if (i >= 0 && i < static_cast<int>(bytes.size()) && (bytes[static_cast<std::size_t>(i)] & 0x30) == 0x10) {
                green = true;
            }
        }
        const int turned = (((place & 0xFF) << 8) | ((place >> 8) & 0xFF)) ^ room;
        const int n = (turned + 1) & 0xFFFF;
        return green ? kGreenPictures + n % kGreenCount : kDesertPictures + n % kDesertCount;
    }

    // Paul comes into the Harkonnens' throne room: the story is over and
    // its last scene begins (at the next click).
    bool endingBegins() {
        if (world_.room() != kThroneOfTheHarkonnens || scene_) {
            return false;
        }
        world_.setU8(kProgress, kStoryOver);
        startScene(kEndingScene);
        return true;
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
        const bool wasAtWar = atWar();
        // The troops on their way step on, but the one given orders, and
        // the others work. The original draws the state's word 0 afresh at
        // every turn of its main loop; the steps turn it for their dice.
        world_.setU16(0, static_cast<std::uint16_t>(g_.random2()));
        world_.hourPassed(orders_.troop(), map_.flatMap());
        if (world_.mapRedraws > 0) {
            world_.mapRedraws = 0;
            map_.redraw();
        }
        if (world_.lostWithPaul) {
            world_.lostWithPaul = false;
            world_.gameOverReason = kPlaceLost;
        }
        if (world_.gameOverReason != 0) {
            const int reason = world_.gameOverReason;
            world_.gameOverReason = 0;
            gameOver(reason);
            return;
        }
        if (world_.troopArrivedHere) {
            // A troop came to the room Paul is in: it is drawn again, with
            // the newcomers, when nothing is shown over it.
            world_.troopArrivedHere = false;
            if (plainView()) {
                d_.targetBack();
                drawView();
                d_.showBack();
                d_.flush();
            }
        }
        d_.targetScreen();
        panel_.drawZones(1, 1);
        panel_.drawClock(world_);
        d_.copyRect(d_.screen(), d_.back(), kClockArea);
        d_.targetBack();
        fadeSky();
        if (atWar() != wasAtWar && !passingHours_) {
            warChanged();
        }
    }

    // --- The sky

    int skyOfTheHour() const {
        const int clock = world_.clock();
        return g_.resources().program().dataU8(kSkyOfTheHour + static_cast<std::size_t>(clock & 0x0F)) +
               ((clock >> 2) & 0x1C);
    }

    // Puts sky `n`'s colours in `to`: from inside a room the colours of
    // the windows, from outside those of the sky; then the panel's.
    void loadSky(int n, Palette& to) { loadSky(n, to, roomSky()); }
    // The sky as a room's view shows it (with Paul inside or walking in
    // the desert), not as from an ornithopter.
    bool roomSky() const { return world_.inside() || !flight_.active(); }
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
        const bool inside = roomSky();
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

    // Hours went by at once: the sky is the hour's, without fading to it.
    void settleSky() {
        if (skyFadeLeft_ > 0) {
            skyFadeLeft_ = 0;
            g_.removeTimer(kSkyTimer);
        }
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
        const bool inside = roomSky();
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
        if (scene_) {
            return world_.u8(kProgress) == kSceneOwnMusicStep ? kSceneOwnMusic : 0;
        }
        if (globe_.active()) {
            return 1;
        }
        if (map_.active()) {
            return 2;
        }
        if (flight_.active() || takingOff_) {
            return flight_.worm ? kFlyingMoment + 1 : kFlyingMoment;
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
        if (g_.musicOff) {
            return;
        }
        if ((g_.musicStyle & kCdStyleBit) != 0) {
            // As a CD plays: the next track once one has ended.
            if (!g_.musicPlaying()) {
                playNextCdTrack();
            }
            return;
        }
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
        if ((g_.musicStyle & kCdStyleBit) != 0) {
            waitingTrack_ = 0;
            return;
        }
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
        talk_.harkonnens = people_.harkonnens;
        const std::vector<World::Character> here = world_.findPresent();
        const bool war = atWar() && !scene_;
        if (scene_) {
            panel_.setArrowZones(kNoArrows);
        } else if (war) {
            panel_.setArrowZones(world_.u8(kWarArrowsSet) != 0 ? kWarArrows : kNoArrows);
        } else {
            panel_.setArrows(world_, places.exits());
        }
        panel_.drawSlots(world_);

        std::vector<Command> commands = scene_ ? sceneCommands() : placeCommands();
        for (const World::Character& c : scene_ ? std::vector<World::Character>{} : here) {
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
        if (!war) {
            chooseMusic(); // at war the battle's sound goes on over the music there was
        }
        places.present = world_.present();
        places.absent = world_.away();
        places.spotTurn = world_.u8(kSpotTurn);
        places.moreChiefs = people_.chiefs.empty() ? 0 : static_cast<int>(people_.chiefs.size()) - 1;
        places.standingList = scene_ ? sceneStanding_ : std::vector<int>{};
        const int room = world_.room();
        if (war) {
            places.forgetShown();
            war_.start(warHorizon());
        } else if (!world_.inside()) {
            places.drawDesert(desertPicture());
            setSky();
        } else if (world_.inside() && room == kSkyRoom) {
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

    // --- War at Paul's place (see docs/rules/fights.md)

    bool atWar() const { return world_.inside() && world_.u8(kAtWar) != 0; }

    std::vector<std::uint16_t> warHorizon() const {
        std::vector<std::uint16_t> list;
        for (std::size_t at = kWarHorizon; at + 6 <= World::kSize && list.size() < 60; at += 6) {
            const std::uint16_t word = world_.u16(at);
            if (word == 0xFFFF) {
                break;
            }
            list.insert(list.end(), {word, world_.u16(at + 2), world_.u16(at + 4)});
        }
        list.push_back(0xFFFF);
        return list;
    }

    // The battle goes on behind what is in front of it; only its flashes
    // show then.
    void pauseWar() {
        war_.paused = talk_.active() || map_.active() || globe_.active() || cockpit_.active() ||
                      flight_.active() || orders_.active() || messageRoom_.active() || over_;
    }

    // The fight at Paul's place began or ended with the hour: the view
    // is drawn again, unless something else is shown.
    // Whether the room is shown with its own commands and nothing over it.
    bool plainView() const {
        return !(talk_.active() || map_.active() || globe_.active() || cockpit_.active() || flight_.active() ||
                 orders_.active() || messageRoom_.active() || mixer_.active() || over_ || scene_ || mirror_ ||
                 planShown_);
    }

    void warChanged() {
        if (!atWar()) {
            war_.stop();
        }
        if (talk_.active() || map_.active() || globe_.active() || cockpit_.active() || flight_.active() ||
            orders_.active() || messageRoom_.active() || over_ || scene_) {
            return;
        }
        d_.targetBack();
        drawView();
        d_.showBack();
        d_.flush();
    }

    // "Massive attack": the fight at once, then the blasts of it.
    void massiveAttack() {
        world_.massiveAttack(map_.flatMap());
        for (int i = 0; i < kMassiveRounds; ++i) {
            const int r = g_.random(0x0201);
            war_.blast((r & 1) == 0 ? 0x0B : 0x11);
            g_.waitTicks(kBlastTicks + ((r >> 8) & 2));
        }
        if (world_.lostWithPaul) {
            world_.lostWithPaul = false;
            world_.gameOverReason = kPlaceLost;
        }
        if (world_.gameOverReason != 0) {
            const int reason = world_.gameOverReason;
            world_.gameOverReason = 0;
            gameOver(reason);
            return;
        }
        if (!atWar()) {
            warChanged();
        }
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
        auto grey = [](Command c, bool greyed) {
            if (greyed) {
                c.phrase = static_cast<std::uint16_t>(c.phrase | Command::kGrey);
            }
            return c;
        };
        if (!world_.inside()) {
            // In the desert.
            list.push_back(commandAt(kSeeMap));
            list.push_back(grey(commandAt(kCallWorm), world_.u8(kProgress) < kWormProgress));
            list.push_back(commandAt(kWaitForEvening + (world_.hour() < kEveningFrom ? 0 : 4)));
            list.push_back(commandAt(kMixer));
            return list;
        }
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
    Vision vision_;
    MessageRoom messageRoom_;
    AttackView war_; // the battle seen from Paul's place at war
    // When the room was last shown: a message waits a while after it.
    bool scene_ = false;
    std::uint16_t sceneAt_ = 0;
    int sceneRoom_ = 0;
    int sceneTalker_ = -1;
    std::vector<int> sceneStanding_;
    std::size_t sceneMenu_ = 0;
    bool sceneBlink_ = false;
    bool planShown_ = false;
    int chosenLine_ = -1; // the panel's line of the last command chosen
    std::uint32_t quietSince_ = 0;
    bool passingHours_ = false;
    World::People people_;
    std::size_t chief_ = 0; // which of the chiefs here Paul talks to
    int walkingTo_ = 0;     // the room Paul went for when someone spoke up
    std::size_t goingTo_ = 0; // the place of the label while "go there" is offered
    bool takingOff_ = false;
    bool endTalk_ = false;
    // A companion's remark or the cockpit's warning is shown.
    bool noticeShown_ = false;
    // The game is over: why, which list of commands is shown, and which of
    // the two recordings is said next.
    bool over_ = false;
    int overReason_ = 0;
    enum class EndList { Main, Leave, Load, Save };
    bool mirror_ = false;
    MixerPanel mixer_;
    MixerList mixerList_ = MixerList::Main;
    std::vector<Command> beforeMixer_; // the commands the panel replaced
    int mixerLit_ = -1;
    std::array<std::uint8_t, kCdTracks> cdOrder_{};
    std::size_t cdNext_ = 0;
    std::uint32_t cdSilentSince_ = 0;
    bool cdSilent_ = false;
    int lastMouseX_ = 0;
    int lastMouseY_ = 0;
    EndList endList_ = EndList::Main;
    bool endTake_ = false;
    bool quit_ = false;
    bool won_ = false; // the story has come to its end
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

GameEnd runGame(Game& game, int language) {
    game.language = language;
    return Play(game).run();
}

} // namespace mentat
