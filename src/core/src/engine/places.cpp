// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/places.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/game.hpp"

#include <algorithm>
#include <string>

namespace mentat {

namespace {

// Places in the program's data.
constexpr std::size_t kPlaceTable = 0x100;
constexpr std::size_t kRoomTables = 0x13C4; // per kind of rooms, where its rooms are listed
constexpr std::size_t kRoomSize = 5;
constexpr std::uint8_t kTableEnd = 0xFF;
constexpr int kMaxPlaces = 100;
constexpr int kSpots = 0x17;

// The scene files, by kind of place: sietch, palace, village, fortress.
int scenesFor(int kind) {
    if (kind < 0x20) {
        return 0;
    }
    if (kind < 0x21) {
        return 1;
    }
    if (kind < 0x28) {
        return 2;
    }
    return 3;
}

} // namespace

Places::Places(Game& game) : game_(game) {
    const ProgramFile& program = game.resources().program();
    for (int i = 0; i < kMaxPlaces; ++i) {
        const std::size_t at = kPlaceTable + 0x1C * static_cast<std::size_t>(i);
        if (program.dataU8(at) == kTableEnd) {
            return;
        }
        PlaceRecord rec;
        for (std::size_t b = 0; b < rec.bytes.size(); ++b) {
            rec.bytes[b] = program.dataU8(at + b);
        }
        records_.push_back(rec);
    }
    throw FormatError("program: the table of places does not end");
}

void Places::enter(int room, int place) {
    room_ = room & 0xFFFF;
    place_ = place & 0xFFFF;
}

std::size_t Places::roomRecord() const {
    const ProgramFile& program = game_.resources().program();
    const int kind = (room_ >> 8) & 0xFF;
    const int number = room_ & 0xFF;
    if (number == 0) {
        throw FormatError("places: no room 0");
    }
    const std::size_t list = program.dataU16(kRoomTables + 2 * static_cast<std::size_t>(kind));
    return list + (static_cast<std::size_t>(number) - 1) * kRoomSize;
}

int Places::roomByte() const {
    return game_.resources().program().dataU8(roomRecord());
}

std::array<std::uint8_t, 4> Places::exits() const {
    const std::size_t at = roomRecord();
    std::array<std::uint8_t, 4> e{};
    for (std::size_t i = 0; i < 4; ++i) {
        e[i] = game_.resources().program().dataU8(at + 1 + i);
    }
    return e;
}

void Places::loadScenes() {
    const int index = ((place_ >> 8) & 0xFF) - 1;
    if (index < 0 || index >= static_cast<int>(records_.size())) {
        throw FormatError("places: no place " + std::to_string(index + 1));
    }
    const int kind = scenesFor(records_[static_cast<std::size_t>(index)].kind());
    if (kind == scenesKind_ && scenes_) {
        return;
    }
    scenes_ = SceneFile::parse(game_.resources().load(resource::kFirstScene + kind));
    scenesKind_ = kind;
}

void Places::drawPerson(int character, const PersonSlot& slot) {
    const int previous = game_.sheetResource();
    game_.selectSheet(kPersons);
    ClipRect bounds{slot.x, slot.y, slot.x, slot.y};
    for (int part = 0; part < 2; ++part) {
        const auto word = static_cast<std::uint16_t>(slot.flags | (character * 2 + part));
        drawSpriteWord(game_.canvas(), game_.sheet(), word, slot.x, slot.y, game_.sceneTables(), slot.mode);
        const ClipRect r = spriteWordBounds(game_.sheet(), word, slot.x, slot.y, game_.sceneTables());
        bounds.right = std::max(bounds.right, r.right);
        bounds.bottom = std::max(bounds.bottom, r.bottom);
    }
    shown_.push_back(Shown{character, bounds});
    game_.selectSheet(previous);
}

int Places::characterAt(int x, int y) const {
    for (auto it = shown_.rbegin(); it != shown_.rend(); ++it) {
        if (x >= it->rect.left && x < it->rect.right && y >= it->rect.top && y < it->rect.bottom) {
            return it->character;
        }
    }
    return -1;
}

void Places::drawView() {
    shown_.clear();
    Display& d = game_.display();
    d.clearRect(d.target(), game_.sceneTables().lineClip);
    const int code = roomByte();
    if (code >= 0x80) {
        throw FormatError("places: the outside views are not built yet");
    }
    loadScenes();
    const RoomNumber number = RoomNumber::decode(code);
    if (number.pictureFile != 0) {
        game_.selectSheet(number.pictureFile);
    }
    const auto scene = scenes_->scene(static_cast<std::size_t>(number.scene));
    if (scene.empty()) {
        throw FormatError("places: empty scene");
    }

    // Who stands where: each character present takes the spot its number
    // (turned by spotTurn) names, or the first free one.
    const int spots = scene[0];
    std::vector<int> standing(kSpots, -1);
    if (spots > 0) {
        unsigned mask = static_cast<unsigned>(present ^ absent);
        for (int character = 0; mask != 0; ++character, mask >>= 1) {
            if ((mask & 1) == 0) {
                continue;
            }
            int spot = (character + (spotTurn & 0x0F)) % spots;
            if (standing[static_cast<std::size_t>(spot)] != -1) {
                spot = 0;
                while (spot < spots && standing[static_cast<std::size_t>(spot)] != -1) {
                    ++spot;
                }
                if (spot >= spots) {
                    continue;
                }
            }
            standing[static_cast<std::size_t>(spot)] = character;
        }
    }
    // The scene names its spots from the last one down.
    drawScene(game_.canvas(), scene, game_.sheet(), game_.sceneTables(), [&](const PersonSlot& slot) {
        const int index = spots - 1 - slot.slot;
        if (index < 0 || index >= kSpots || standing[static_cast<std::size_t>(index)] < 0) {
            return;
        }
        drawPerson(standing[static_cast<std::size_t>(index)], slot);
    });
}

} // namespace mentat
