// SPDX-License-Identifier: GPL-3.0-or-later
//
// Flying the ornithopter to a place: the desert going by in the videos, a
// small map in the corner with the trail behind, a step on the map every
// so often, and the approach when the place is reached. See
// docs/rules/flight.md.
#pragma once

#include "mentat/engine/small_map.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace mentat {

class Game;
class MapScreen;
class Panel;
class World;

class Flight {
public:
    // The original's numbers for what its commands do.
    static constexpr std::uint16_t kSkip = 0x4FFB;
    static constexpr std::uint16_t kChange = 0x497A;
    static constexpr std::uint16_t kBack = 0x50A5;
    static constexpr std::uint16_t kNearest = 0x50C4;
    static constexpr std::uint16_t kLeft = 0x4AD0;
    static constexpr std::uint16_t kRight = 0x4AD7;

    Flight(Game& game, World& world, Panel& panel, MapScreen& map);

    // From leaving the ground until landing, the cockpit opened on the way
    // included.
    bool active() const { return active_; }
    std::size_t destination() const { return destination_; }

    // Sets course for place `record` from where the player is.
    void setDestination(std::size_t record);
    // Flies freely toward the map point (longitude, latitude) and on past
    // it; the place last entered is where the flight would end.
    void setFree(std::uint16_t longitude, int latitude);
    bool free() const { return free_; }
    // Leaves the ground: the trail starts and the first step is taken.
    void begin();
    // Shows the flight from the start of its first video: the commands,
    // the small map in the corner and the trail.
    void show();
    // While the cockpit is open the flight waits.
    bool paused = false;

    // While the game waits: the video's frames, with the corner over them,
    // and a step every 768 ticks.
    void tick();
    // Parts of the day that steps added to the clock since the last look.
    int hoursPassed = 0;
    // The place is reached; `approach` when it is reached flying (rather
    // than by skipping).
    bool arrived = false;
    bool approach = false;

    // A command of the flight's list; false when it is not one of them.
    // Sets `changing` when the cockpit should open.
    bool command(std::uint16_t action);
    bool changing = false;

    // The approach to a sietch or a palace: the flight's videos go on until
    // the right moment, then the place's own video plays through.
    // False when the place has no such video.
    bool playApproach();
    // Ends the flight in the place reached.
    void land();
    // Ends the flight where it is: the game is over.
    void stop();
    // How the place is reached, by its kind: 6 a sietch's video, 7 a
    // palace's, 9 a fort's; 8 and 10 the ornithopter lands in front.
    int approachVideo() const;
    static constexpr int kFortVideo = 9;
    // Plays video slot `slot` through on the scene.
    void playVideo(int slot);

    // What the last step came upon, for the game to show: a companion
    // seeing a hidden place, or the Harkonnens' zone. The flight waits
    // until it is answered.
    enum class Notice { None, Sighting, Zone };
    Notice notice = Notice::None;
    // The companion who speaks of it; -1 when nobody goes with Paul (the
    // cockpit warns of the zone instead).
    int noticedBy = -1;
    // Too long in the Harkonnens' zone: Paul is caught.
    bool caught = false;
    // The notice is answered: "GO TOWARDS THIS PLACE" or "IGNORE WARNING"
    // (then the flight is shown again) or "CHANGE DESTINATION".
    void endNotice();

    // Steps for tests and tools.
    void step();
    // The heading, 256 to a turn: 0 north, then clockwise.
    int heading() const { return heading_; }
    // The video slot playing.
    int videoSlot() const { return slot_; }

private:
    void advance(std::uint16_t& longitude, int& latitude);
    void move(std::uint16_t& longitude, int& latitude);
    void steer(int latitude);
    void lookAround(std::uint16_t longitude, int latitude);
    void spotPlaces(std::uint16_t longitude, int latitude);
    void checkZone(std::uint16_t longitude, int latitude);
    void greySkip(bool grey);
    void drawCommands();
    bool headingTo(std::uint16_t longitude, int latitude, std::size_t record, int& heading) const;
    static bool headingToward(std::uint16_t longitude, int latitude, std::uint16_t toLongitude, int toLatitude,
                              int& heading);
    std::size_t nearestPlace() const;
    int terrain(std::uint16_t longitude, int latitude) const;
    int pointIndex(std::uint16_t longitude, int latitude) const;
    void lookAhead();
    void chooseNextVideo(int terrain);
    void nextVideo();
    void showFrame();
    void drawCorner();
    void drawTrail();
    void drawCornerSprite(std::uint16_t sprite, int x, int y);
    void addToTrail(std::uint16_t longitude, int latitude);

    Game& g_;
    World& world_;
    Panel& panel_;
    MapScreen& map_;
    SmallMap small_;
    bool active_ = false;
    // Flying in a direction, and steered by hand (the arrows).
    bool free_ = false;
    bool manual_ = false;
    std::size_t destination_ = 0;
    int heading_ = 0;
    int fraction_ = 0x80;
    int steps_ = 0;
    // Steps left in the Harkonnens' zone, counted down in 32s from 256;
    // 0 outside it.
    int zoneCount_ = 0;
    bool skipGrey_ = false;
    std::uint32_t lastStep_ = 0;
    // The corner: shown and up to date (0), to be drawn again around the
    // player (1), or not shown (-1).
    int corner_ = 0;
    // The videos: the one playing, the next, and the one the chain goes
    // back to.
    int slot_ = 0;
    int next_ = 0;
    int base_ = 2;
    // The trail: the points flown over, the oldest overwritten.
    static constexpr std::size_t kTrailLength = 276;
    std::array<std::pair<std::uint16_t, int>, kTrailLength> trail_{};
    std::array<bool, kTrailLength> trailUsed_{};
    std::size_t trailNext_ = 0;
};

} // namespace mentat
