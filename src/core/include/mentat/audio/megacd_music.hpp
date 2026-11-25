// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Mega CD version's music: its own Z80 sound program, taken from the
// player's Mega CD disc, run on an emulated Z80 next to an emulated YM2612
// (the Mega Drive's FM chip), and fed its pieces the way the console's main
// processor feeds them. See docs/rules/music-megacd.md. Built only when
// Mentat is built with the two emulators (MTT_WITH_MEGACD).
#pragma once

#include "mentat/audio/music_system.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace mentat {

// What the Mega CD music needs from the player's disc: the sound program
// and the pieces, numbered from 1 as the disc numbers them.
struct MegaCdMusicFiles {
    static constexpr int kPieces = 9;
    std::vector<std::uint8_t> program;
    std::array<std::vector<std::uint8_t>, kPieces> pieces;
};

// Reads MCDPROG.BIN and MCDSONG1.BIN to MCDSONG9.BIN from `folder` (see
// importMegaCdMusic). Throws FormatError when one cannot be read.
MegaCdMusicFiles readMegaCdMusic(const std::filesystem::path& folder);

class MegaCdMusic : public MusicSystem {
public:
    // Throws FormatError when the files do not look like the disc's.
    explicit MegaCdMusic(MegaCdMusicFiles files);
    ~MegaCdMusic() override;
    MegaCdMusic(const MegaCdMusic&) = delete;
    MegaCdMusic& operator=(const MegaCdMusic&) = delete;

    // The Mega CD piece played where the CD version plays its piece
    // `track`; 0 for none.
    static int pieceFor(int track);

    MusicCard card() const override { return MusicCard::MegaCd; }
    bool playsOwnPieces() const override { return true; }
    void startPiece(int track, int plays) override;
    // The CD version's pieces do not play here; see startPiece.
    void start(std::shared_ptr<const Song>, int) override {}
    void stop() override;
    void endAfterThisPlay() override {}
    void setVolume(std::uint8_t volume, std::uint8_t balance) override;
    void fade(int ticks, std::uint8_t volume, std::uint8_t balance) override;
    const MusicStatus& status() const override { return status_; }
    void runTo(std::uint32_t tick, std::vector<std::int16_t>& out) override;
    int rate() const override;

    // Runs the sound program alone for `seconds`, as a test would.
    void runSeconds(double seconds, std::vector<std::int16_t>& out);

private:
    struct Board;
    std::unique_ptr<Board> board_;
    std::array<std::vector<std::uint8_t>, MegaCdMusicFiles::kPieces> pieces_;
    MusicStatus status_;
    std::uint8_t attenuation_ = 0;
    std::uint32_t tick_ = 0;
    double base_ = 0;
    bool started_ = false;
};

} // namespace mentat
