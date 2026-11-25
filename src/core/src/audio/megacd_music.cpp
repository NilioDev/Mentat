// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/audio/megacd_music.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/engine/platform.hpp"

#include <ym3438.h>
#include <z80.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <iterator>
#include <string>

namespace mentat {

namespace {

// The console's clocks.
constexpr double kZ80Clock = 3579545.0;
constexpr double kFmClock = 7670453.0;
// The FM chip makes a sound sample every 24 of its steps, each 6 of its
// clock's beats.
constexpr int kStepsPerSample = 24;
constexpr double kStepsPerZ80Tick = kFmClock / 6.0 / kZ80Clock;
constexpr double kVblankTicks = kZ80Clock / 60.0;

// Where things are in the sound program's memory (see the rules).
constexpr std::size_t kProgramSize = 0x1F73;
constexpr std::uint16_t kCommand = 0x0003;
constexpr std::uint16_t kArgs = 0x0004;
constexpr std::uint16_t kDataReady = 0x000A;
constexpr std::uint16_t kWindows = 0x000C; // 0x18 bytes for each part
constexpr std::size_t kParts = 9;
constexpr std::uint16_t kWindowSize = 0x18;
constexpr std::uint16_t kHeader = 0x00E4;
constexpr std::uint16_t kTempo = 0x0114;
constexpr std::uint16_t kRequestsLow = 0x0120;  // parts 0-4
constexpr std::uint16_t kRequestsHigh = 0x01F2; // parts 5-8
constexpr std::uint16_t kCursorsLow = 0x01DE;
constexpr std::uint16_t kCursorsHigh = 0x02B0;
constexpr std::uint16_t kBlock = 0x02BA;
constexpr std::uint16_t kBlockSize = 0x0F74;
constexpr std::uint16_t kState = 0x125F; // bit 7: playing
constexpr std::size_t kPieceSize = 0x80F0;
// The main processor's byte the program sets when it wants more of the
// piece; the program is told where it is.
constexpr std::uint32_t kFlagAddress = 0xFFFF24A0;

enum Command : std::uint8_t {
    kTellFlag = 1,
    kPlay = 3,
    kStop = 4,
    kFadeTo = 6,
};

} // namespace

struct MegaCdMusic::Board : z80::z80_cpu<Board> {
    std::array<std::uint8_t, 0x2000> ram{};
    std::array<std::uint8_t, 2> flag{}; // the main processor's two bytes
    std::uint16_t bank = 0;
    ym3438_t fm{};
    double steps = 0;
    int stepInSample = 0;
    std::int32_t left = 0;
    std::int32_t right = 0;
    std::vector<std::int16_t>* out = nullptr;
    std::uint64_t ticks = 0;
    double nextVblank = 0;

    // The main processor's side.
    std::vector<std::uint8_t> piece;
    std::array<std::uint32_t, kParts> parts{};
    std::array<bool, kParts> hasPart{};
    bool feeding = false;
    struct Pending {
        std::uint8_t command;
        std::uint8_t a;
        std::uint8_t b;
        std::vector<std::uint8_t> piece;
    };
    std::deque<Pending> pending;

    std::uint32_t windowAddress(std::uint16_t addr) const {
        return (static_cast<std::uint32_t>(bank) << 15) | (addr & 0x7FFFu);
    }

    z80::fast_u8 on_read(z80::fast_u16 addr) {
        if (addr < 0x4000) {
            return ram[addr & 0x1FFF];
        }
        if (addr < 0x6000) {
            return OPN2_Read(&fm, addr & 3);
        }
        if (addr >= 0x8000) {
            const std::uint32_t at = windowAddress(static_cast<std::uint16_t>(addr)) & 0xFFFFFF;
            if (at >= (kFlagAddress & 0xFFFFFF) && at < (kFlagAddress & 0xFFFFFF) + 2) {
                return flag[at - (kFlagAddress & 0xFFFFFF)];
            }
        }
        return 0xFF;
    }

    void on_write(z80::fast_u16 addr, z80::fast_u8 n) {
        const auto value = static_cast<std::uint8_t>(n);
        if (addr < 0x4000) {
            ram[addr & 0x1FFF] = value;
        } else if (addr < 0x6000) {
            OPN2_Write(&fm, addr & 3, value);
        } else if (addr < 0x6100) {
            // The bank register takes one bit at a time, filling from the
            // top.
            bank = static_cast<std::uint16_t>(((bank >> 1) | ((value & 1u) << 8)) & 0x1FF);
        } else if (addr >= 0x8000) {
            const std::uint32_t at = windowAddress(static_cast<std::uint16_t>(addr)) & 0xFFFFFF;
            if (at >= (kFlagAddress & 0xFFFFFF) && at < (kFlagAddress & 0xFFFFFF) + 2) {
                flag[at - (kFlagAddress & 0xFFFFFF)] = value;
            }
        }
        // The PSG (0x7F11) is not used by the music.
    }

    void on_tick(unsigned t) {
        z80::z80_cpu<Board>::on_tick(t);
        ticks += t;
        steps += t * kStepsPerZ80Tick;
        while (steps >= 1.0) {
            steps -= 1.0;
            Bit16s sample[2];
            OPN2_Clock(&fm, sample);
            left += sample[0];
            right += sample[1];
            if (++stepInSample == kStepsPerSample) {
                stepInSample = 0;
                if (out != nullptr) {
                    out->push_back(clip(left));
                    out->push_back(clip(right));
                }
                left = right = 0;
            }
        }
    }

    static std::int16_t clip(std::int32_t v) {
        v *= 5;
        return static_cast<std::int16_t>(std::clamp(v, -32768, 32767));
    }

    void put(std::uint16_t at, const std::uint8_t* from, std::size_t size) {
        std::copy(from, from + size, ram.begin() + at);
    }

    void runUntil(double tick) {
        while (static_cast<double>(ticks) < tick) {
            if (static_cast<double>(ticks) >= nextVblank) {
                vblank();
                nextVblank += kVblankTicks;
            }
            on_step();
        }
    }

    // What the main processor does at each frame: one waiting command, and
    // more of the piece when the program asked.
    void vblank() {
        if (!pending.empty() && ram[kCommand] == 0) {
            Pending p = std::move(pending.front());
            pending.pop_front();
            if (p.command == kPlay) {
                loadPiece(std::move(p.piece));
            }
            ram[kArgs + 4] = p.a;
            ram[kArgs + 5] = p.b;
            ram[kCommand] = p.command;
            return;
        }
        if (feeding && flag[0] != 0) {
            refill();
        }
    }

    void loadPiece(std::vector<std::uint8_t> bytes) {
        piece = std::move(bytes);
        piece.resize(kPieceSize + kBlockSize, 0);
        const auto word = [&](std::size_t at) { return static_cast<std::uint32_t>(piece[at] | (piece[at + 1] << 8)); };
        put(kBlock, piece.data() + word(0), kBlockSize);
        for (std::size_t i = 0; i < kParts; ++i) {
            const std::uint32_t offset = word(2 + 2 * i);
            hasPart[i] = offset != 0;
            parts[i] = 2 + offset;
            if (hasPart[i]) {
                put(static_cast<std::uint16_t>(kWindows + kWindowSize * i), piece.data() + parts[i], kWindowSize);
            }
        }
        put(kHeader, piece.data() + 2, 0x30);
        const std::uint32_t tempo = word(0x32);
        ram[kTempo] = static_cast<std::uint8_t>(0x100 - tempo * 0x4C / 0x480);
        feeding = true;
    }

    void refill() {
        std::uint16_t requests = kRequestsLow;
        std::uint16_t cursors = kCursorsLow;
        for (std::size_t i = 0; i < kParts; ++i) {
            const auto window = static_cast<std::uint16_t>(kWindows + kWindowSize * i);
            const std::uint16_t asked = static_cast<std::uint16_t>(ram[requests] | (ram[requests + 1] << 8));
            requests = static_cast<std::uint16_t>(requests + 2);
            if (asked != 0) {
                const std::uint16_t offset = static_cast<std::uint16_t>(asked - 1);
                const auto cursor = static_cast<std::uint16_t>(window - offset);
                ram[cursors] = static_cast<std::uint8_t>(cursor);
                ram[cursors + 1] = static_cast<std::uint8_t>(cursor >> 8);
                cursors = static_cast<std::uint16_t>(cursors + 2);
                if (requests == kRequestsLow + 10) {
                    requests = kRequestsHigh;
                    cursors = kCursorsHigh;
                }
                const std::size_t from = (hasPart[i] ? parts[i] : 0) + offset;
                for (std::uint16_t k = 0; k < kWindowSize; ++k) {
                    ram[window + k] = from + k < piece.size() ? piece[from + k] : 0;
                }
            }
        }
        ram[kDataReady] = 0xFF;
        flag[0] = 0;
    }
};

namespace {

std::vector<std::uint8_t> readAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw FormatError("cannot read " + path.filename().string());
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

} // namespace

MegaCdMusicFiles readMegaCdMusic(const std::filesystem::path& folder) {
    MegaCdMusicFiles files;
    files.program = readAll(folder / "MCDPROG.BIN");
    for (int i = 0; i < MegaCdMusicFiles::kPieces; ++i) {
        files.pieces[static_cast<std::size_t>(i)] = readAll(folder / ("MCDSONG" + std::to_string(i + 1) + ".BIN"));
    }
    return files;
}

MegaCdMusic::MegaCdMusic(MegaCdMusicFiles files) : board_(std::make_unique<Board>()) {
    if (files.program.size() < kProgramSize || files.program[0] != 0xC3) {
        throw FormatError("the Mega CD sound program is not right");
    }
    for (const auto& piece : files.pieces) {
        if (piece.size() < 0x40 || piece[2] != 0x32 || piece[3] != 0x00) {
            throw FormatError("a Mega CD piece is not right");
        }
    }
    pieces_ = std::move(files.pieces);
    Board& b = *board_;
    std::copy(files.program.begin(), files.program.begin() + kProgramSize, b.ram.begin());
    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Reset(&b.fm);
    b.set_pc(0);
    // The program starts, then is told where the main processor's byte is.
    b.runUntil(kVblankTicks);
    b.ram[kArgs + 0] = static_cast<std::uint8_t>(kFlagAddress >> 24);
    b.ram[kArgs + 1] = static_cast<std::uint8_t>(kFlagAddress >> 16);
    b.ram[kArgs + 2] = static_cast<std::uint8_t>(kFlagAddress >> 8);
    b.ram[kArgs + 3] = static_cast<std::uint8_t>(kFlagAddress);
    b.ram[kCommand] = kTellFlag;
    b.runUntil(3 * kVblankTicks);
}

MegaCdMusic::~MegaCdMusic() = default;

int MegaCdMusic::pieceFor(int track) {
    // The CD version's tracks 1-10 (see docs/rules/music-megacd.md).
    static constexpr std::array<int, 11> kPieces{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
    return track >= 0 && track < static_cast<int>(kPieces.size()) ? kPieces[static_cast<std::size_t>(track)] : 0;
}

void MegaCdMusic::startPiece(int track, int) {
    const int piece = pieceFor(track);
    if (piece == 0) {
        stop();
        return;
    }
    board_->pending.push_back(
        {kPlay, static_cast<std::uint8_t>(piece), attenuation_, pieces_[static_cast<std::size_t>(piece - 1)]});
    status_.flags = MusicStatus::kPlaying;
    status_.measure = 1;
}

void MegaCdMusic::stop() {
    board_->pending.push_back({kStop, 0, 0, {}});
    status_.flags = 0;
}

void MegaCdMusic::setVolume(std::uint8_t volume, std::uint8_t) {
    // The disc's volume steps are 8 apart, 0 the loudest and 0x7F none.
    attenuation_ = static_cast<std::uint8_t>(std::min(0x7F, (0xFF - volume) >> 1));
    if (status_.playing()) {
        board_->pending.push_back({kFadeTo, attenuation_, 0xFF, {}});
    }
}

void MegaCdMusic::fade(int ticks, std::uint8_t volume, std::uint8_t) {
    attenuation_ = static_cast<std::uint8_t>(std::min(0x7F, (0xFF - volume) >> 1));
    // The pattern of the program's steps that move the volume: every one
    // for a short fade, fewer for longer ones.
    const std::uint8_t pace = ticks < 60 ? 0xFF : (ticks < 240 ? 0x55 : 0x11);
    board_->pending.push_back({kFadeTo, attenuation_, pace, {}});
}

int MegaCdMusic::rate() const { return static_cast<int>(std::lround(kFmClock / 144.0)); }

void MegaCdMusic::runTo(std::uint32_t tick, std::vector<std::int16_t>& out) {
    if (!started_) {
        started_ = true;
        tick_ = tick;
        base_ = static_cast<double>(board_->ticks) - tick * (kZ80Clock / kTicksPerSecond);
        return;
    }
    board_->out = &out;
    board_->runUntil(base_ + tick * (kZ80Clock / kTicksPerSecond));
    board_->out = nullptr;
    tick_ = tick;
    if ((board_->ram[kState] & 0x80) == 0 && board_->pending.empty()) {
        status_.flags = 0;
    }
}

void MegaCdMusic::runSeconds(double seconds, std::vector<std::int16_t>& out) {
    board_->out = &out;
    board_->runUntil(static_cast<double>(board_->ticks) + seconds * kZ80Clock);
    board_->out = nullptr;
}

} // namespace mentat
