// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's FM music files (for the AdLib and the AdLib Gold) and the
// tables of its FM music players. See docs/formats/music.md,
// docs/rules/music.md and docs/rules/music-gold.md.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class DatArchive;

class Song {
public:
    // Voice lists the header has room for; the AdLib player plays the
    // first 9, the AdLib Gold player all 18.
    static constexpr int kVoices = 18;
    static constexpr std::size_t kInstrumentSize = 40;
    // The AdLib Gold settings after the header: the stereo processor's
    // switch byte, then the 31 surround registers.
    static constexpr std::size_t kGoldSettings = 52;
    static constexpr std::size_t kGoldSettingsSize = 32;

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

    // The byte at `at`, 0 past the end.
    std::uint8_t byteOr0(std::size_t at) const { return at < bytes_.size() ? bytes_[at] : 0; }

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

enum class MusicCard {
    SoundBlasterPro, // DNADP.HSQ with the .HSQ music
    AdLibGold,       // DNADG.HSQ with the .AGD music
    Mt32,            // DNMID.HSQ with the .M32 music, on a Roland MT-32
};

// The tables inside one of the original's FM music players.
struct MusicTables {
    static constexpr int kAdLibVoices = 9;

    MusicCard card = MusicCard::SoundBlasterPro;
    std::array<std::uint16_t, 12> frequency{};
    std::array<std::uint8_t, 13> distance{};
    std::array<std::uint8_t, 5> fineLow{};
    std::array<std::uint8_t, 5> fineHigh{};
    // Per voice, where its registers are: the channel, the channel three up
    // (the second half of a four-operator sound) and the two operators. The
    // top bit picks the chip's second register set. The AdLib Gold player
    // changes these as it gives voices channels; these are its first ones.
    std::array<std::uint8_t, Song::kVoices> channel{};
    std::array<std::uint8_t, Song::kVoices> pairedChannel{};
    std::array<std::uint8_t, Song::kVoices> firstOperator{};
    std::array<std::uint8_t, Song::kVoices> secondOperator{};
    // The frequency each voice's channel gets when the player stops before
    // the voice played.
    std::array<std::uint16_t, Song::kVoices> firstFrequency{};

    int voices() const { return card == MusicCard::AdLibGold ? Song::kVoices : kAdLibVoices; }

    // Takes the unpacked DNADP.HSQ. Throws FormatError.
    static MusicTables fromPlayer(std::span<const std::uint8_t> player);
    // Takes the unpacked DNADG.HSQ. Throws FormatError.
    static MusicTables fromGoldPlayer(std::span<const std::uint8_t> player);
    // The player for `card` from DUNE.DAT; the Sound Blaster Pro one when
    // the files have no AdLib Gold player. Throws FormatError.
    static MusicTables load(DatArchive& archive, MusicCard card);
};

} // namespace mentat
