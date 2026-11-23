// SPDX-License-Identifier: GPL-3.0-or-later
//
// Walking in the desert. See docs/rules/desert.md.
#include "mentat/engine/world.hpp"

#include <algorithm>

namespace mentat {

namespace {

constexpr std::size_t kPlaceNumber = 0x09;
constexpr std::size_t kTiredness = 0xF4;
constexpr std::size_t kFoundUnconscious = 0xE7;
constexpr int kMostTired = 0x14;
constexpr int kMostSteps = 0x7F;
constexpr int kSouthEdge = 0x62;  // the latitudes Paul can walk to: −0x61 to 0x61
constexpr int kNorthEdge = -0x62;
constexpr int kSietchesFrom = 0x20; // below: a sietch; 0x20 the palace

} // namespace

bool World::walkDesert(int way) {
    std::uint16_t longitude = 0;
    int latitude = 0; // the latitude, as the place word's low byte
    int fraction = 0; // the 256ths of a latitude walked south of it
    if (inside()) {
        setU8(kFoundUnconscious, 0);
        const std::size_t record = placeHere();
        setPlaceHere(0);
        longitude = u16(record + 2);
        latitude = static_cast<std::int8_t>(u8(record + 4));
        setU8(kPlaceKind, kFlying);
        setU8(kPlaceNumber, 0xFF);
    } else {
        longitude = static_cast<std::uint16_t>(room());
        latitude = static_cast<std::int8_t>(place() & 0xFF);
        fraction = (place() >> 8) & 0xFF;
        desertSteps_ = std::min(desertSteps_ + 1, kMostSteps);
        if (u8(kTiredness) < kMostTired) {
            setU8(kTiredness, static_cast<std::uint8_t>(u8(kTiredness) + 1));
        }
    }

    // A step north or south is a 256th of a latitude; east or west, the
    // smallest unit of longitude.
    switch (way) {
    case 1:
        if (fraction > 0) {
            --fraction;
        } else if (latitude - 1 > kNorthEdge) {
            --latitude;
            fraction = 0xFF;
        }
        break;
    case 3:
        if (fraction < 0xFF) {
            ++fraction;
        } else if (latitude + 1 < kSouthEdge) {
            ++latitude;
            fraction = 0;
        }
        break;
    case 2: ++longitude; break;
    case 4: --longitude; break;
    default: break;
    }

    // On a place's own point, Paul comes into it.
    if (fraction == 0) {
        for (std::size_t r = kPlaces; u16(r) != 0xFFFF; r += kPlaceSize) {
            if (u16(r + 2) == longitude && static_cast<std::int16_t>(u16(r + 4)) == latitude) {
                enterPlace(r);
                return true;
            }
        }
    }
    const int where = (fraction << 8) | (latitude & 0xFF);
    takeFollowers(longitude, where);
    setU8(kLastRoom, u8(kRoomNumber));
    setU8(kRoomNumber, static_cast<std::uint8_t>(longitude & 0xFF));
    setU16(kRoom, longitude);
    setU16(kPlace, static_cast<std::uint16_t>(where));
    return false;
}

void World::carriedHome() {
    const std::size_t home = u16(kHome);
    enterPlace(home);
    // In a sietch to its second room, in the palace to its tenth.
    const int kind = u8(home + 8);
    if (kind <= kSietchesFrom) {
        const int room = (kind << 8) | (kind == kSietchesFrom ? 0x0A : 0x02);
        goTo(room, place());
    }
    setU8(kFoundUnconscious, static_cast<std::uint8_t>(u8(kFoundUnconscious) + 1));
}

} // namespace mentat
