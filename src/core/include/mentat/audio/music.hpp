// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's AdLib music files and the tables of its FM music player.
// See docs/formats/music.md and docs/rules/music.md.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class Song {
public:
    static constexpr int kVoices = 9;
    static constexpr std::size_t kInstrumentSize = 40;

    // Takes the unpacked file. Throws FormatError.
    static Song parse(std::vector<std::uint8_t> bytes);

    std::uint16_t tempo() const { return tempo_; }
    std::uint16_t repeatStart() const { return repeatStart_; }
    std::uint16_t repeatEnd() const { return repeatEnd_; }
    std::uint16_t repeatCount() const { return repeatCount_; }

    // Where voice v's events start in bytes(), 0 when the voice is unused.
    std::size_t voiceStart(int voice) const { return voices_[static_cast<std::size_t>(voice)]; }
    std::size_t instrumentCount() const { return instrumentCount_; }
    // Instrument i, 40 bytes. Throws FormatError.
    std::span<const std::uint8_t> instrument(std::size_t index) const;

    const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::array<std::size_t, kVoices> voices_{};
    std::size_t instruments_ = 0;
    std::size_t instrumentCount_ = 0;
    std::uint16_t tempo_ = 0;
    std::uint16_t repeatStart_ = 0;
    std::uint16_t repeatEnd_ = 0;
    std::uint16_t repeatCount_ = 0;
};

// The tables inside the original's FM music player, DNADP.HSQ.
struct MusicTables {
    std::array<std::uint16_t, 12> frequency{};
    std::array<std::uint8_t, Song::kVoices> firstOperator{};
    std::array<std::uint8_t, Song::kVoices> secondOperator{};
    std::array<std::uint8_t, 13> distance{};
    std::array<std::uint8_t, 5> fineLow{};
    std::array<std::uint8_t, 5> fineHigh{};

    // Takes the unpacked DNADP.HSQ. Throws FormatError.
    static MusicTables fromPlayer(std::span<const std::uint8_t> player);
};

} // namespace mentat
