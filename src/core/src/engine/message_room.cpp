// SPDX-License-Identifier: GPL-3.0-or-later
//
// The palace's message room. See docs/rules/news.md.
#include "mentat/engine/message_room.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/engine/display.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/talk.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/engine/world.hpp"

namespace mentat {

namespace {

constexpr int kSheet = 0x15; // the device's and the holograms' sprites
// The device: its sprite list (three entries of sprite, x, y), the glow
// over it and the sparks on its screen.
constexpr std::size_t kDeviceList = 0x14C8;
constexpr int kFirstGlow = 0x0B;
constexpr int kGlowX = 0x64;
constexpr int kGlowY = 0x56;
constexpr int kFirstSpark = 0x17;
constexpr int kSparkX = 0xA3;
constexpr int kSparkTop = 0x67;
constexpr int kSparkBottom = 0x70;
constexpr int kSparkRow = 3;
constexpr int kFirstSidePiece = 0x13;
constexpr int kFrameTicks = 9;
constexpr int kOnFrames = 20;
constexpr int kAppearFrames = 13;
constexpr int kVanishFrom = 4;
// The holograms: sprite 0x1E + who sent the news, where the table of
// places to show people (data offset 0x225D, a word each: x, then y)
// puts them.
constexpr int kFirstHologram = 0x1E;
constexpr std::size_t kHologramPlaces = 0x225D;
// Sounds and the computer's voice.
constexpr int kHologramSound = 0x0A;
constexpr int kVanishSound = 0x09;
constexpr int kComputer = 0x19;
constexpr int kNewVoice = 0x2A; // and 0x2B for the news already seen
constexpr int kCancelVoice = 0x29;
constexpr int kFirstSenderVoice = 0x1A;
// The list: a line for each piece of news, named after who sent it.
constexpr std::uint16_t kFirstPersonPhrase = 0x78;
constexpr std::uint16_t kCancelPhrase = 0xA3;
// The state: which list is shown (0xFF the news seen), who sent the news
// talked about, what it is about, how many were new when one was viewed,
// and what just happened (6 once news is viewed).
constexpr std::size_t kListShown = 0xDB;
constexpr std::size_t kSender = 0xE9;
constexpr std::size_t kAbout = 0x24;
constexpr std::size_t kNewNews = 0xC9;
constexpr std::size_t kNewWhenViewed = 0xEB;
constexpr std::size_t kSituation = 0x23;
constexpr std::uint8_t kViewedSituation = 6;
// News about this means the Emperor's troops take the planet.
constexpr int kEmperorsTroops = 0x0C;
constexpr int kEmperorsEnd = 7;
constexpr ClipRect kScene{0, 0, 320, 152};

} // namespace

MessageRoom::MessageRoom(Game& game, World& world, Panel& panel, Talk& talk)
    : g_(game), world_(world), panel_(panel), talk_(talk) {
    const ProgramFile& program = g_.resources().program();
    for (std::size_t i = 0; i < device_.size(); ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            device_[i][j] = program.dataU16(kDeviceList + 6 * i + 2 * j);
        }
    }
}

bool MessageRoom::command(std::uint16_t action, int line) {
    switch (action) {
    case kNew:
    case kSeen:
        open(action == kSeen);
        return true;
    case kPick:
        if (open_) {
            pick(line);
        }
        return true;
    case kCancel:
        if (open_) {
            cancel();
        }
        return true;
    case Talk::kViewed:
        if (open_) {
            viewed();
        }
        return true;
    default:
        return false;
    }
}

void MessageRoom::say(int phrase) { g_.talker().playVoice(kComputer, phrase); }

void MessageRoom::open(bool seen) {
    seen_ = seen;
    open_ = true;
    world_.setU8(kListShown, seen ? 0xFF : 0);
    say(kNewVoice + (seen ? 1 : 0));
    // The device lights up and its screen sparkles.
    drawDevice(1);
    drawGlow(2);
    for (int frame = 0; frame < kOnFrames; ++frame) {
        sparkle(frame);
        g_.waitTicks(kFrameTicks);
    }
    drawGlow(1);
    world_.dropNewsReminders();
    world_.setU8(kSender, 0);

    std::vector<Command> list;
    for (const int at : world_.newsShown(seen)) {
        const std::uint16_t news = world_.u16(0x1179 + 2 * static_cast<std::size_t>(at));
        list.push_back(Command{static_cast<std::uint16_t>(kFirstPersonPhrase + (news & 0x3F)), kPick});
    }
    list.push_back(Command{kCancelPhrase, kCancel});
    panel_.setCommands(std::move(list));
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
}

void MessageRoom::pick(int line) {
    const std::vector<int> shown = world_.newsShown(seen_);
    if (line < 0 || static_cast<std::size_t>(line) >= shown.size()) {
        return;
    }
    const std::uint16_t news = world_.lookAtNews(shown[static_cast<std::size_t>(line)], seen_);
    const int sender = news & 0x3F;
    if (sender != hologram_) {
        hideHologram();
        say(sender + kFirstSenderVoice);
        showHologram(sender);
    }
    world_.setU8(kSender, static_cast<std::uint8_t>(sender));
    const std::uint16_t at = g_.resources().program().dataU16(kHologramPlaces + 2 * static_cast<std::size_t>(sender));
    talk_.news(sender, at & 0xFF, at >> 8);
}

void MessageRoom::viewed() {
    world_.setU8(kNewWhenViewed, world_.u8(kNewNews));
    if (hologram_ == 0) {
        return;
    }
    talk_.endRemark();
    // The hologram shows over the face, then fades away with it.
    Display& d = g_.display();
    d.targetScreen();
    drawHologram();
    d.targetBack();
    const bool ends = world_.u8(kAbout) == kEmperorsTroops;
    hideHologram();
    world_.setU8(kAbout, 0);
    world_.setU8(kSituation, kViewedSituation);
    open_ = false;
    if (backToRoom) {
        backToRoom();
    }
    if (ends && ended) {
        ended(kEmperorsEnd);
    }
}

void MessageRoom::cancel() {
    say(kCancelVoice);
    open_ = false;
    if (backToRoom) {
        backToRoom();
    }
}

// --- The device

void MessageRoom::drawDevice(int glow) {
    Display& d = g_.display();
    d.targetScreen();
    g_.selectSheet(kSheet);
    for (const auto& s : device_) {
        g_.drawSprite(s[0], s[1], s[2]);
    }
    d.targetBack();
    drawGlow(glow);
}

void MessageRoom::drawGlow(int glow) {
    Display& d = g_.display();
    d.targetScreen();
    g_.selectSheet(kSheet);
    g_.drawSprite(static_cast<std::uint16_t>(kFirstGlow + (glow & 7)), kGlowX, kGlowY);
    d.targetBack();
}

// A frame of the sparks: a random one (not the one above it) on each row
// of the device's screen, and the side pieces turned.
void MessageRoom::sparkle(int frame) {
    Display& d = g_.display();
    d.targetScreen();
    g_.selectSheet(kSheet);
    int last = -1;
    for (int y = kSparkTop; y <= kSparkBottom; y += kSparkRow) {
        int spark = g_.random(0x0F);
        while (spark == last) {
            spark = g_.random(0x0F);
        }
        last = spark;
        g_.drawSprite(static_cast<std::uint16_t>(kFirstSpark + spark), kSparkX, y);
    }
    for (std::size_t i = 1; i < device_.size(); ++i) {
        device_[i][0] = static_cast<std::uint16_t>((device_[i][0] & 0xFF00) | (kFirstSidePiece + (frame & 3)));
        g_.drawSprite(device_[i][0], device_[i][1], device_[i][2]);
    }
    d.targetBack();
}

// A frame of a hologram coming or going: the glow rises and falls with the
// frame (2, 3, 4, 5, 6, 5, 4, 3) and the sparks go on.
void MessageRoom::step(int frame) {
    int glow = frame & 7;
    if (glow >= 5) {
        glow = 8 - glow;
    }
    drawGlow(glow + 2);
    sparkle(frame);
}

// Waits until the frame that began at `due` has lasted its ticks.
void MessageRoom::wait(std::uint32_t& due) {
    due += kFrameTicks;
    const std::uint32_t now = g_.platform().ticks;
    if (static_cast<std::int32_t>(due - now) > 0) {
        g_.waitTicks(static_cast<int>(due - now));
    }
}

void MessageRoom::drawHologram() {
    g_.selectSheet(kSheet);
    g_.playSound(kHologramSound);
    const std::uint16_t at =
        g_.resources().program().dataU16(kHologramPlaces + 2 * static_cast<std::size_t>(hologram_));
    g_.drawSprite(static_cast<std::uint16_t>(kFirstHologram + hologram_), at & 0xFF, at >> 8);
}

void MessageRoom::showHologram(int sender) {
    hologram_ = sender;
    std::uint32_t due = g_.platform().ticks;
    for (int frame = 0; frame < kAppearFrames; ++frame) {
        step(frame);
        wait(due);
    }
    Display& d = g_.display();
    d.copyRect(d.screen(), d.back(), kScene);
    d.targetBack();
    drawHologram();
    d.transition(Display::kSparkle);
}

void MessageRoom::hideHologram() {
    if (hologram_ == 0) {
        return;
    }
    Display& d = g_.display();
    d.copyRect(d.screen(), d.back(), kScene);
    d.targetBack();
    if (drawRoom) {
        drawRoom();
    }
    d.transition(Display::kSparkle);
    hologram_ = 0;
    g_.selectSheet(kSheet);
    g_.playSound(kVanishSound);
    std::uint32_t due = g_.platform().ticks;
    for (int frame = kVanishFrom; frame >= 0; --frame) {
        step(frame);
        wait(due);
    }
    drawDevice(1);
}

} // namespace mentat
