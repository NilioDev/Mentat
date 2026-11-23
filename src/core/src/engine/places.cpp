// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/places.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

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

// The palace's rooms lie in the state, where the story opens their ways.
std::uint8_t Places::roomData(std::size_t at) const {
    if (stateByte) {
        if (const auto b = stateByte(at)) {
            return *b;
        }
    }
    return game_.resources().program().dataU8(at);
}

int Places::roomByte() const {
    return roomData(roomRecord());
}

std::array<std::uint8_t, 4> Places::exits() const {
    const std::size_t at = roomRecord();
    std::array<std::uint8_t, 4> e{};
    for (std::size_t i = 0; i < 4; ++i) {
        e[i] = roomData(at + 1 + i);
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
    int down = 0;
    const int person = personOf ? personOf(character, down) : character;
    const int y = slot.y + down;
    ClipRect bounds{slot.x, y, slot.x, y};
    for (int part = 0; part < 2; ++part) {
        const auto word = static_cast<std::uint16_t>(slot.flags | (person * 2 + part));
        drawSpriteWord(game_.canvas(), game_.sheet(), word, slot.x, y, game_.sceneTables(), slot.mode);
        const ClipRect r = spriteWordBounds(game_.sheet(), word, slot.x, y, game_.sceneTables());
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

void Places::drawSky() {
    constexpr int kRows = 4;
    constexpr int kTileWidth = 40;
    constexpr int kTileHeight = 20;
    game_.selectSheet(resource::kSky);
    for (int row = 0; row < kRows; ++row) {
        for (int x = 0; x < Surface::kScreenWidth; x += kTileWidth) {
            game_.drawSprite(static_cast<std::uint16_t>(row), x, row * kTileHeight);
        }
    }
}

namespace {

// The first rooms' pictures, by the kind of place: the picture file and how
// many there are, for a sietch, the palace, a village, the fortress and
// the rest; for the villages, one by name.
constexpr std::size_t kOutsidePictures = 0x1972;
constexpr std::size_t kOutsideCounts = 0x1977;
constexpr int kKindSteps[4] = {0x20, 0x21, 0x28, 0x30};
constexpr int kVillagePictures = 0x7F;
constexpr int kOrnithopters = 0x33; // ORNY.HSQ
constexpr int kTakingOff = 0x2A;    // ORNYTK.HSQ: the same, with the wings beating
constexpr ClipRect kSceneArea{0, 0, 320, 152};

} // namespace

void Places::drawOutside(int kind, int firstName) {
    const int picture = outsidePicture(kind, firstName, 0);
    if (picture < 0) {
        return;
    }
    game_.selectSheet(picture);
    game_.drawSprite(0, 0, 0);
}

int Places::outsidePicture(int kind, int firstName, int step) const {
    const ProgramFile& program = game_.resources().program();
    std::size_t k = 4;
    for (std::size_t i = 0; i < 4; ++i) {
        if (kind < kKindSteps[i]) {
            k = i;
            break;
        }
    }
    if (step >= program.dataU8(kOutsideCounts + k)) {
        return -1;
    }
    int picture = program.dataU8(kOutsidePictures + k) + step;
    if (picture == kVillagePictures) {
        picture += (firstName >> 1) - 5;
    }
    return picture;
}

void Places::drawDesert(int picture) {
    shown_.clear();
    Display& d = game_.display();
    d.clearRect(d.target(), game_.sceneTables().lineClip);
    game_.selectSheet(picture);
    game_.drawSprite(0, 0, 0);
}

// One ornithopter with its top-left at (x, y), `state` steps into taking
// off: the shadow shrinks from step 15, the wings beat up to step 14.
void Places::drawOrnithopter(int x, int y, int state) {
    const SpriteSheet& sheet = game_.sheet();
    auto draw = [&](std::size_t n, int at, int top) {
        drawSpriteClipped(game_.canvas(), sheet.sprite(n), at, top, kSceneArea);
    };
    draw(0, x, y);                                                  // the body
    draw(1, x + 6, y + 30);                                         // below it
    draw(static_cast<std::size_t>(2 + std::clamp(state - 15, 0, 5)), x + 4, y + 50); // its shadow
    draw(static_cast<std::size_t>(8 + std::min(state, 14)), x - 0x51, y - 3);        // the wings
}

void Places::drawOrnithopters(int count, bool sietch) {
    game_.selectSheet(kOrnithopters);
    drawRow(0, count, sietch);
}

// The parked ornithopters from the `first` to the last of `count`, each
// further right and lower than the one before.
void Places::drawRow(int first, int count, bool sietch) {
    const int x = sietch ? 0x95 : 0xCA;
    const int y = sietch ? 0x39 : 0x49;
    for (int i = first; i < count; ++i) {
        drawOrnithopter(x + 0x46 * i, y + 0x0A * i, 0);
    }
}

void Places::drawTakeOff(int count, bool sietch, int state, int side) {
    game_.selectSheet(kTakingOff);
    int x = sietch ? 0x95 : 0xCA;
    int y = sietch ? 0x39 : 0x49;
    // From step 15 it rises, sideways and faster and faster.
    const int n = state - 14;
    if (n > 0) {
        x -= 5 * side * n;
        y -= n * n / 2;
    }
    drawOrnithopter(x, y, state);
    drawRow(1, count, sietch);
}

void Places::drawView(const std::function<void()>& background) {
    shown_.clear();
    Display& d = game_.display();
    d.clearRect(d.target(), game_.sceneTables().lineClip);
    if (background) {
        background();
    }
    const int code = roomByte();
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
    if (!standingList.empty()) {
        // A scene says who stands where.
        for (std::size_t i = 0; i < standingList.size() && i < standing.size(); ++i) {
            standing[i] = standingList[i];
        }
    } else if (spots > 0) {
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
        // The other chiefs take the spots after, the same way.
        for (int k = 1; k <= moreChiefs; ++k) {
            const int character = 15 + k;
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
