// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/flight.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <cstdlib>
#include <vector>

namespace mentat {

namespace {

constexpr int kPanelSheet = 0;

// A step every 768 ticks; every 16 steps a part of the day passes.
constexpr std::uint32_t kStepTicks = 0x300;
constexpr int kStepsPerHour = 16;
// The latitude where the flight turns back from a pole.
constexpr int kPoleTurn = 0x60;

// The commands while flying to a place, and the arrows (none); flying
// freely, the way back, the nearest place from a point of the story on,
// and the arrows to turn.
constexpr std::size_t kSkipCommand = 0x21FC;
constexpr std::size_t kChangeCommand = 0x21F8;
constexpr std::size_t kBackCommand = 0x2200;
constexpr std::size_t kNearestCommand = 0x2204;
constexpr std::size_t kFlightArrows = 0x1D1E;
constexpr std::size_t kFreeArrows = 0x1D72;
constexpr std::size_t kProgress = 0x2A;
constexpr int kNearestFrom = 0x32;
// An arrow turns the heading by this much.
constexpr int kTurn = 4;
// Flying freely, beyond these latitudes a heading toward the pole turns
// east or west.
constexpr int kPoleSteer = 0x4D;

// On the way: the state's byte for what is happening (3 a place seen, 4
// the Harkonnens' zone), those who go with Paul and the two shown on the
// panel, and the words a companion's line names: the side (0 left, 1
// right), the kind of place and the side as phrases.
constexpr std::size_t kSituation = 0x23;
constexpr std::uint8_t kSighting = 3;
constexpr std::uint8_t kInZone = 4;
constexpr std::size_t kWith = 0x10;
constexpr std::size_t kSlots = 0x1152;
constexpr std::size_t kStateFlags = 0x00;
constexpr std::size_t kSide = 0xE1;
constexpr std::size_t kSeenKind = 0x11F3;
constexpr std::size_t kSeenSide = 0x11F5;
constexpr std::uint16_t kKindWords = 0x48; // "a sietch", "a palace", "a village", "a fortress"
constexpr std::uint16_t kLeftWord = 0xCE;  // "on the left"; "on the right" two further
// A companion looks over the 9 × 9 points around Paul, for a place
// within this much of the heading either way.
constexpr int kLookAround = 9;
constexpr int kSeenWithin = 0x60;
constexpr int kTopRow = -0x62;
// Map bytes: a place at the point; both bits for the Harkonnens' zone.
constexpr std::uint8_t kPlaceAtPoint = 0x40;
constexpr std::uint8_t kZoneBits = 0x30;
// Each step in the zone counts this much down from 256.
constexpr int kZoneStep = 0x20;
// A destination that is safe to fly to through the zone: a fortress
// with this bit in its byte 0x0A.
constexpr int kFirstFortress = 0x28;
constexpr std::uint8_t kSafeFortress = 0x08;

// The corner: the small map's rectangle, and the part of the screen the
// corner covers, from the third buffer, over each frame of the videos.
constexpr std::size_t kCornerMap = 0x148A;
constexpr std::size_t kCornerArea = 0x1492;
// Its frame: four rings, from the map outwards.
constexpr std::uint8_t kFrameColours[4] = {0xFC, 0xFA, 0xF8, 0xF6};
// Sprites of the panel's picture file: the destination, a point of the
// trail, the player.
constexpr std::uint16_t kDestinationSprite = 0x2E;
constexpr std::uint16_t kTrailSprite = 0x2F;
constexpr std::uint16_t kPlayerSprite = 0x30;
// While the player is inside this rectangle the map stays; once out of
// it, the map is drawn again around the player at the next step.
constexpr ClipRect kKeepCentred{0xD6, 0x0A, 0x132, 0x36};
// Trail points are drawn when they fall within this rectangle.
constexpr ClipRect kTrailArea{0xCC, 0x04, 0x13A, 0x3A};

// The videos of the desert going by: dunes in slots 2 and 5, rocks in 3
// and 4; a map point of rocks has its low four bits at 8 or more.
constexpr int kFirstDunes = 2;
constexpr int kRocks = 8;
// Seven steps ahead are looked at to choose the next video.
constexpr int kLookAhead = 6;
// The approach: a sietch's video or a palace's, after this frame of the
// first video of dunes.
constexpr int kFirstApproach = 6;
constexpr int kFirstLanding = 8;
constexpr int kSietchApproachFrame = 0x3C;
constexpr int kOtherApproachFrame = 0x16;
// A place's kind (byte 8): from these on, the next kind of approach.
constexpr int kKindSteps[4] = {0x20, 0x21, 0x28, 0x30};

// "SKIP TO DESTINATION" goes at most this many steps.
constexpr int kSkipSteps = 200;

std::int16_t s16(std::uint16_t v) { return static_cast<std::int16_t>(v); }

// The heading's direction as (across, down), each from -32 to 32.
void vectorOf(int heading, int& across, int& down) {
    const int turned = (heading + 0x20) & 0xFF;
    int a = heading & 0xFF;
    if ((turned & 0x7F) >= 0x40) {
        across = 0x20;
        a = (a - 0x40) & 0xFF;
        if (turned & 0x80) {
            across = -0x20;
            a = -((a - 0x80) & 0xFF) & 0xFF;
        }
        down = static_cast<std::int8_t>(a);
    } else {
        down = -0x20;
        if (turned & 0x80) {
            a = -((a - 0x80) & 0xFF) & 0xFF;
            down = 0x20;
        }
        across = static_cast<std::int8_t>(a);
    }
}

} // namespace

Flight::Flight(Game& game, World& world, Panel& panel, MapScreen& map)
    : g_(game), world_(world), panel_(panel), map_(map), small_(game, world, map) {}

// --- Where to

bool Flight::headingTo(std::uint16_t longitude, int latitude, std::size_t record, int& heading) const {
    return headingToward(longitude, latitude, world_.u16(record + 2), s16(world_.u16(record + 4)), heading);
}

bool Flight::headingToward(std::uint16_t longitude, int latitude, std::uint16_t toLongitude, int toLatitude,
                           int& heading) {
    int down = toLatitude - latitude;
    int across = s16(static_cast<std::uint16_t>(toLongitude - longitude));
    if (down < -0x80 || down >= 0x80) {
        down >>= 1;
        across >>= 1;
    }
    // The latitudes count 256 times as much as the longitudes.
    const int scaledDown = s16(static_cast<std::uint16_t>((down & 0xFF) << 8));
    const int downSize = std::abs(scaledDown);
    const int acrossSize = std::abs(across);
    int angle = 0;
    if (acrossSize >= downSize) {
        if (acrossSize < 1) {
            return false;
        }
        angle = 0x20 * scaledDown / across + (across >= 0 ? 0x40 : 0xC0);
    } else {
        if (downSize < 1) {
            return false;
        }
        angle = 0x20 * across / scaledDown;
        if (scaledDown >= 0) {
            angle -= 0x80;
        }
        angle = -angle;
    }
    heading = angle & 0xFF;
    return true;
}

void Flight::setDestination(std::size_t record) {
    destination_ = record;
    free_ = false;
    manual_ = false;
    std::uint16_t lon = 0;
    int lat = 0;
    world_.position(lon, lat);
    int h = 0;
    heading_ = headingTo(lon, lat, record, h) ? h : 0;
    fraction_ = 0x80;
}

void Flight::setFree(std::uint16_t longitude, int latitude) {
    free_ = true;
    manual_ = true;
    destination_ = world_.home();
    std::uint16_t lon = 0;
    int lat = 0;
    world_.position(lon, lat);
    int h = 0;
    if (headingToward(lon, lat, longitude, latitude, h)) {
        heading_ = h;
    }
    fraction_ = 0x80;
}

// The heading toward the destination from where the player is; flying
// freely only away from the poles.
void Flight::steer(int latitude) {
    if (free_ || manual_) {
        if (latitude >= -kPoleSteer && latitude <= kPoleSteer) {
            return;
        }
        const bool north = ((heading_ - 0x40) & 0x80) != 0;
        if (north != (latitude < 0)) {
            return; // already away from the pole
        }
        heading_ = (heading_ & 0x80) | 0x40;
        return;
    }
    std::uint16_t lon = 0;
    int lat = 0;
    world_.position(lon, lat);
    int h = 0;
    if (headingTo(lon, lat, destination_, h)) {
        heading_ = h;
    }
}

void Flight::move(std::uint16_t& longitude, int& latitude) {
    steer(latitude);
    // A step goes a map point's width at the equator, and the same
    // distance in longitude units nearer the poles.
    const int speed = map_.flatMap().pointSize(std::abs(latitude));
    int across = 0;
    int down = 0;
    vectorOf(heading_, across, down);
    int alongStep = speed * across / 32;
    const int downStep = down * speed / 32;
    // A latitude at most a step: what is left over waits for the next.
    int total = std::abs(downStep) + fraction_;
    if ((total >> 8) > 1) {
        alongStep = alongStep * 256 / total;
        total = 0x100;
    }
    fraction_ = total & 0xFF;
    const int downMove = downStep < 0 ? -(total >> 8) : (total >> 8);
    latitude += downMove;
    longitude = static_cast<std::uint16_t>(longitude + alongStep);
    // Near a pole the flight turns round, over to the other side.
    if (latitude < -kPoleTurn || latitude >= kPoleTurn) {
        heading_ = (heading_ + 0x80) & 0xFF;
        longitude = static_cast<std::uint16_t>(longitude + 0x8000);
    }
}

int Flight::pointIndex(std::uint16_t longitude, int latitude) const {
    const FlatMap& m = map_.flatMap();
    return m.rowStart(latitude) + m.pointAt(longitude, latitude);
}

int Flight::terrain(std::uint16_t longitude, int latitude) const {
    const auto& bytes = map_.flatMap().bytes();
    const int at = pointIndex(longitude, latitude);
    return at >= 0 && static_cast<std::size_t>(at) < bytes.size() ? bytes[static_cast<std::size_t>(at)] : 0;
}

// --- Flying

void Flight::begin() {
    active_ = true;
    arrived = false;
    approach = false;
    changing = false;
    paused = false;
    notice = Notice::None;
    noticedBy = -1;
    caught = false;
    hoursPassed = 0;
    steps_ = 0;
    base_ = kFirstDunes;
    slot_ = 0;
    next_ = kFirstDunes;
    trailUsed_.fill(false);
    trailNext_ = 0;
    // The first step leaves the place at once; the next follows at the
    // first look.
    std::uint16_t lon = 0;
    int lat = 0;
    advance(lon, lat);
    lastStep_ = g_.platform().ticks - kStepTicks;
}

void Flight::show() {
    Display& d = g_.display();
    paused = false;
    d.targetBack();
    drawCommands();
    corner_ = 0;
    std::uint16_t lon = 0;
    int lat = 0;
    world_.position(lon, lat);
    small_.longitude = lon;
    small_.latitude = lat;
    drawCorner();
    drawTrail();
    slot_ = base_;
    d.targetBack();
    g_.openVideo(slot_);
    showFrame();
    d.showBack();
    d.flush();
}

// The commands and arrows: to a place, or flying freely.
void Flight::drawCommands() {
    const ProgramFile& program = g_.resources().program();
    auto commandAt = [&](std::size_t at) { return Command{program.dataU16(at), program.dataU16(at + 2)}; };
    std::vector<Command> list;
    if (!free_) {
        Command skip = commandAt(kSkipCommand);
        if (skipGrey_) {
            skip.phrase = static_cast<std::uint16_t>(skip.phrase | Command::kGrey);
        }
        list.push_back(skip);
    } else {
        list.push_back(commandAt(kBackCommand));
        if (world_.u8(kProgress) >= kNearestFrom) {
            list.push_back(commandAt(kNearestCommand));
        }
    }
    list.push_back(commandAt(kChangeCommand));
    panel_.setCommands(std::move(list));
    panel_.drawCommands();
    panel_.setArrowZones(free_ ? kFreeArrows : kFlightArrows);
}

void Flight::tick() {
    if (!active_ || paused || arrived) {
        return;
    }
    Display& d = g_.display();
    d.targetBack();
    if (g_.videoDone()) {
        nextVideo();
    } else if (g_.videoStep()) {
        showFrame();
    }
    if (g_.platform().ticks - lastStep_ >= kStepTicks) {
        lastStep_ = g_.platform().ticks;
        step();
    }
}

// The corner over the frame just drawn, and the scene on the screen.
void Flight::showFrame() {
    Display& d = g_.display();
    if (corner_ >= 0) {
        const ProgramFile& program = g_.resources().program();
        auto at = [&](std::size_t o) { return static_cast<int>(s16(program.dataU16(kCornerArea + o))); };
        d.copyRect(d.aux(), d.back(), ClipRect{at(0), at(2), at(4), at(6)});
    }
    d.copyRect(d.back(), d.screen(), g_.sceneTables().lineClip);
}

void Flight::nextVideo() {
    slot_ = next_;
    Display& d = g_.display();
    d.targetBack();
    g_.openVideo(slot_);
    d.flush();
    showFrame();
}

// The video after this one goes by what lies ahead: dunes or rocks.
void Flight::chooseNextVideo(int ground) {
    if (base_ < kFirstDunes) {
        next_ = base_;
        return;
    }
    if ((ground & 0x0F) < kRocks) {
        if (slot_ <= 2) {
            next_ = base_;
        } else if (slot_ <= 4) {
            next_ = 5;
        } else {
            base_ = kFirstDunes;
            next_ = kFirstDunes;
        }
    } else {
        next_ = slot_ <= 2 || slot_ == 5 ? 3 : 4;
    }
}

void Flight::lookAhead() {
    std::uint16_t lon = 0;
    int lat = 0;
    world_.position(lon, lat);
    const int keep = fraction_;
    for (int i = 0; i < kLookAhead; ++i) {
        move(lon, lat);
    }
    const int sixth = terrain(lon, lat);
    move(lon, lat);
    fraction_ = keep;
    const int seventh = terrain(lon, lat);
    chooseNextVideo(((sixth + seventh) & 0xFF) >> 1);
}

// A step on the map, the clock moving on with the steps.
void Flight::advance(std::uint16_t& lon, int& lat) {
    if ((++steps_ % kStepsPerHour) == 0) {
        world_.setClock(world_.clock() + 1);
        ++hoursPassed;
    }
    world_.position(lon, lat);
    move(lon, lat);
    world_.flyOver(lon, lat);
}

void Flight::step() {
    std::uint16_t lon = 0;
    int lat = 0;
    advance(lon, lat);

    // The trail: a point where the player was, then where the player is.
    if (corner_ >= 0 && trailUsed_[(trailNext_ + kTrailLength - 1) % kTrailLength]) {
        const auto& [tLon, tLat] = trail_[(trailNext_ + kTrailLength - 1) % kTrailLength];
        int x = 0;
        int y = 0;
        if (small_.toScreen(tLon, tLat, x, y)) {
            drawCornerSprite(kTrailSprite, x - 1, y - 1);
        }
    }
    addToTrail(lon, lat);

    if (pointIndex(lon, lat) == world_.u16(destination_ + 6)) {
        arrived = true;
        approach = true;
        return;
    }
    lookAround(lon, lat);
    if (notice != Notice::None && noticedBy >= 0) {
        // A companion speaks: the rest waits for the flight to go on.
        return;
    }
    if (corner_ >= 0) {
        if (corner_ == 1) {
            corner_ = 0;
            small_.longitude = lon;
            small_.latitude = lat;
            drawCorner();
            drawTrail();
        }
        int x = 0;
        int y = 0;
        if (small_.toScreen(lon, lat, x, y)) {
            if (x < kKeepCentred.left || y < kKeepCentred.top || x >= kKeepCentred.right || y >= kKeepCentred.bottom) {
                corner_ = 1;
            } else {
                drawCornerSprite(kPlayerSprite, x - 1, y - 1);
            }
        }
    }
    lookAhead();
}

// --- On the way

// What is around after a step: a companion may see a hidden place, and
// the Harkonnens' zone may begin.
void Flight::lookAround(std::uint16_t lon, int lat) {
    notice = Notice::None;
    noticedBy = -1;
    world_.setU8(kSituation, 0);
    const std::uint16_t slots = world_.u16(kSlots);
    const bool company = slots != 0xFFFF;
    if (company && world_.u16(kWith) != 0) {
        spotPlaces(lon, lat);
    }
    checkZone(lon, lat);
    const std::uint8_t situation = world_.u8(kSituation);
    if (situation == 0) {
        return;
    }
    if (company) {
        // The one who speaks: the second companion shown, unless there is
        // none or the state's bit says the first.
        int who = slots & 0xFF;
        if ((slots >> 8) != 0xFF && !(world_.u16(kStateFlags) & 0x80)) {
            who = slots >> 8;
        }
        if (who & 0x80) {
            return;
        }
        noticedBy = who;
    }
    notice = situation == kSighting ? Notice::Sighting : Notice::Zone;
    paused = true;
}

// The places in the 9 × 9 points around Paul: one that is hidden, that
// the story has come to, and that lies ahead within a third of a turn
// either way, is seen. It counts as found, and course is set for it.
void Flight::spotPlaces(std::uint16_t lon, int lat) {
    const FlatMap& m = map_.flatMap();
    const auto& bytes = m.bytes();
    const int top = std::max(lat - kLookAround / 2, kTopRow);
    for (int row = top; row < top + kLookAround; ++row) {
        const int ring = 2 * m.width(std::abs(row));
        const int start = m.rowStart(row);
        int column = m.pointAt(lon, row) - kLookAround / 2;
        if (column < 0) {
            column += ring;
        }
        for (int i = 0; i < kLookAround; ++i, ++column) {
            if (column >= ring) {
                column -= ring;
            }
            const int at = start + column;
            if (at < 0 || static_cast<std::size_t>(at) >= bytes.size() || !(bytes[static_cast<std::size_t>(at)] & kPlaceAtPoint)) {
                continue;
            }
            std::size_t record = 0;
            for (std::size_t r = World::kPlaces; world_.u16(r) != 0xFFFF; r += World::kPlaceSize) {
                if (world_.u16(r + 6) == at) {
                    record = r;
                    break;
                }
            }
            if (record == 0 || !(world_.u8(record + 0x0A) & 0x80) || world_.u8(kProgress) < world_.u8(record + 0x0B)) {
                continue;
            }
            int h = 0;
            if (!headingTo(lon, lat, record, h)) {
                continue;
            }
            const int off = (h - heading_ + kSeenWithin) & 0xFF;
            if (off >= 2 * kSeenWithin) {
                continue;
            }
            const int side = off >= kSeenWithin ? 1 : 0;
            world_.setU8(kSide, static_cast<std::uint8_t>(side));
            world_.setU16(kSeenSide, static_cast<std::uint16_t>(kLeftWord + 2 * side));
            world_.setU16(kSeenKind, static_cast<std::uint16_t>(kKindWords + World::kindWord(world_.u8(record + 8))));
            world_.setU8(kSituation, kSighting);
            world_.findPlace(record);
            setDestination(record);
            g_.closeVideo();
            Display& d = g_.display();
            d.targetScreen();
            drawCommands();
            d.targetBack();
            drawCommands();
        }
    }
}

// The Harkonnens' zone: the first step into it warns; after 8 steps in a
// row Paul is caught. Flying to a fortress so marked is safe.
void Flight::checkZone(std::uint16_t lon, int lat) {
    const bool safe = !free_ && world_.u8(destination_ + 8) >= kFirstFortress &&
                      (world_.u8(destination_ + 0x0A) & kSafeFortress);
    if (safe || (terrain(lon, lat) & kZoneBits) != kZoneBits) {
        zoneCount_ = 0;
        greySkip(false);
        return;
    }
    if (zoneCount_ == 0) {
        world_.setU8(kSituation, kInZone);
    }
    greySkip(true);
    zoneCount_ = (zoneCount_ - kZoneStep) & 0xFF;
    if (zoneCount_ == 0) {
        caught = true;
    }
}

// "SKIP TO DESTINATION" is greyed in the zone: from the next time the
// list is drawn, and in the list shown when it heads it.
void Flight::greySkip(bool grey) {
    skipGrey_ = grey;
    std::vector<Command> list = panel_.commands();
    if (!list.empty() && list.front().action == kSkip) {
        list.front().phrase = static_cast<std::uint16_t>((list.front().phrase & ~Command::kGrey) | (grey ? Command::kGrey : 0));
        panel_.setCommands(std::move(list));
    }
}

void Flight::endNotice() {
    notice = Notice::None;
    noticedBy = -1;
    world_.setU8(kSituation, 0);
}

void Flight::addToTrail(std::uint16_t longitude, int latitude) {
    trail_[trailNext_] = {longitude, latitude};
    trailUsed_[trailNext_] = true;
    trailNext_ = (trailNext_ + 1) % kTrailLength;
}

// --- The corner

void Flight::drawCornerSprite(std::uint16_t sprite, int x, int y) {
    Display& d = g_.display();
    d.targetAux();
    g_.selectSheet(kPanelSheet);
    drawSpriteClipped(g_.canvas(), g_.sheet().sprite(sprite), x, y, small_.view);
    d.targetBack();
}

// The small map around its middle, its frame, the places and the
// destination, in the third buffer.
void Flight::drawCorner() {
    const ProgramFile& program = g_.resources().program();
    auto at = [&](std::size_t o) { return static_cast<int>(s16(program.dataU16(kCornerMap + o))); };
    small_.view = ClipRect{at(0), at(2), at(4), at(6)};
    Display& d = g_.display();
    d.targetAux();
    small_.drawMap();
    Surface& s = d.aux();
    ClipRect ring = small_.view;
    for (const std::uint8_t colour : kFrameColours) {
        --ring.left;
        --ring.top;
        for (int x = ring.left; x <= ring.right; ++x) {
            s.set(x, ring.top, colour);
            s.set(x, ring.bottom, colour);
        }
        for (int y = ring.top; y <= ring.bottom; ++y) {
            s.set(ring.left, y, colour);
            s.set(ring.right, y, colour);
        }
        ++ring.right;
        ++ring.bottom;
    }
    g_.selectSheet(kPanelSheet);
    small_.drawPlaces();
    int x = 0;
    int y = 0;
    if (small_.toScreen(world_.u16(destination_ + 2), s16(world_.u16(destination_ + 4)), x, y)) {
        drawSpriteClipped(g_.canvas(), g_.sheet().sprite(kDestinationSprite), x - 1, y - 1, small_.view);
    }
    d.targetBack();
}

// Every point of the trail that falls on the corner's map.
void Flight::drawTrail() {
    for (std::size_t i = 0; i < kTrailLength; ++i) {
        const std::size_t n = (trailNext_ + i) % kTrailLength;
        if (!trailUsed_[n]) {
            continue;
        }
        int x = 0;
        int y = 0;
        if (!small_.toScreen(trail_[n].first, trail_[n].second, x, y)) {
            continue;
        }
        --x;
        --y;
        if (x >= kTrailArea.left && y >= kTrailArea.top && x < kTrailArea.right && y < kTrailArea.bottom) {
            drawCornerSprite(kTrailSprite, x, y);
        }
    }
}

// --- Commands

bool Flight::command(std::uint16_t action) {
    if (!active_ || paused || arrived) {
        return false;
    }
    if (action == kChange) {
        changing = true;
        corner_ = 1;
        return true;
    }
    if (free_ && (action == kLeft || action == kRight)) {
        heading_ = (heading_ + (action == kLeft ? -kTurn : kTurn)) & 0xFF;
        manual_ = true;
        fraction_ = 0x80;
        return true;
    }
    if (free_ && (action == kBack || action == kNearest)) {
        if (action == kBack) {
            setDestination(world_.home());
            // The video starts again.
            g_.display().targetBack();
            g_.openVideo(slot_);
            g_.display().flush();
            showFrame();
        } else {
            setDestination(nearestPlace());
        }
        Display& d = g_.display();
        d.targetScreen();
        drawCommands();
        d.targetBack();
        drawCommands();
        return true;
    }
    if (action != kSkip) {
        return false;
    }
    // Straight there: the steps at once, without the videos; after 200 the
    // place is reached anyway.
    g_.closeVideo();
    manual_ = false;
    for (int i = 0; i < kSkipSteps; ++i) {
        std::uint16_t lon = 0;
        int lat = 0;
        advance(lon, lat);
        if (pointIndex(lon, lat) == world_.u16(destination_ + 6)) {
            break;
        }
    }
    arrived = true;
    approach = false;
    return true;
}

// The place shown on the map nearest to the player: the larger of the
// longitude's distance in 256ths and the latitude's.
std::size_t Flight::nearestPlace() const {
    std::uint16_t lon = 0;
    int lat = 0;
    world_.position(lon, lat);
    std::size_t nearest = world_.home();
    int best = 0xFFFF;
    for (std::size_t r = World::kPlaces; world_.u16(r) != 0xFFFF; r += World::kPlaceSize) {
        if (world_.u8(r + 0x0A) & 0x80) {
            continue;
        }
        const int along = std::abs(static_cast<int>(s16(static_cast<std::uint16_t>(world_.u16(r + 2) - lon))));
        const int down = std::abs(s16(world_.u16(r + 4)) - lat);
        const int far = (along >> 8) >= (down & 0xFF) ? (along >> 8) : down;
        if (far < best) {
            best = far;
            nearest = r;
        }
    }
    return nearest;
}

// --- Arriving

void Flight::land() {
    constexpr std::size_t kOrnithopters = 0x15;
    constexpr std::size_t kDice = 0xC5;
    world_.setU8(kDice, static_cast<std::uint8_t>(g_.random2() & 0xFF));
    active_ = false;
    arrived = false;
    world_.setU8(destination_ + kOrnithopters, static_cast<std::uint8_t>(world_.u8(destination_ + kOrnithopters) + 1));
    world_.enterPlace(destination_);
}

void Flight::stop() {
    active_ = false;
    paused = false;
    notice = Notice::None;
    noticedBy = -1;
    caught = false;
}

int Flight::approachVideo() const {
    const int kind = world_.u8(destination_ + 8);
    int slot = kFirstApproach;
    for (const int step : kKindSteps) {
        slot += kind >= step ? 1 : 0;
    }
    return slot;
}

void Flight::playVideo(int slot) {
    Display& d = g_.display();
    d.targetBack();
    g_.openVideo(slot);
    d.flush();
    d.showBack();
    while (!g_.videoDone()) {
        d.targetBack();
        if (g_.videoStep()) {
            d.copyRect(d.back(), d.screen(), g_.sceneTables().lineClip);
        }
        g_.pollInput();
        g_.platform().waitFrame();
    }
    g_.closeVideo();
}

bool Flight::playApproach() {
    const int slot = approachVideo();
    if (slot >= kFirstLanding) {
        return false;
    }
    Display& d = g_.display();
    corner_ = -1;
    const int frame = slot == kFirstApproach ? kSietchApproachFrame : kOtherApproachFrame;
    // The flight's videos go on, over dunes, until that frame of the first.
    for (;;) {
        d.targetBack();
        if (g_.videoDone()) {
            nextVideo();
        } else if (g_.videoStep()) {
            showFrame();
        }
        chooseNextVideo(0);
        if (slot_ == kFirstDunes && g_.videoFrame() == frame) {
            break;
        }
        g_.pollInput();
        g_.platform().waitFrame();
    }
    next_ = slot;
    nextVideo();
    while (!g_.videoDone()) {
        d.targetBack();
        if (g_.videoStep()) {
            showFrame();
        }
        g_.pollInput();
        g_.platform().waitFrame();
    }
    g_.closeVideo();
    return true;
}

} // namespace mentat
