// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Roland MT-32 music: the original's MIDI music player
// (mentat/audio/midi_driver.hpp) playing on Munt's MT-32 emulator, which
// needs the player's own MT-32 ROM files. Built only when Mentat is built
// with Munt (MTT_WITH_MT32).
#pragma once

#include "mentat/audio/midi_driver.hpp"
#include "mentat/audio/music_system.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace mentat {

// Tells from its contents whether a file is a whole MT-32 or CM-32L ROM,
// and returns the name Mentat keeps it under (see findMt32Roms):
// MT32_CONTROL.ROM, MT32_PCM.ROM, CM32L_CONTROL.ROM or CM32L_PCM.ROM. Empty
// for any other file, so the player's files can be named anything.
std::string mt32RomName(const std::filesystem::path& file);

class Mt32Music : public MusicSystem {
public:
    // Opens the emulator with the two ROM files. Throws FormatError when
    // they cannot be read or are not MT-32 ROMs.
    Mt32Music(const std::filesystem::path& controlRom, const std::filesystem::path& pcmRom);
    ~Mt32Music() override;
    Mt32Music(const Mt32Music&) = delete;
    Mt32Music& operator=(const Mt32Music&) = delete;

    MusicCard card() const override { return MusicCard::Mt32; }
    void start(std::shared_ptr<const Song> song, int plays) override;
    void stop() override;
    void endAfterThisPlay() override { driver_.endAfterThisPlay(); }
    void setVolume(std::uint8_t volume, std::uint8_t balance) override;
    void fade(int ticks, std::uint8_t volume, std::uint8_t balance) override;
    const MusicStatus& status() const override { return driver_.status(); }
    void runTo(std::uint32_t tick, std::vector<std::int16_t>& out) override;
    int rate() const override;

private:
    struct Synth;
    std::unique_ptr<Synth> synth_;
    MidiMusicDriver driver_;
    std::uint32_t tick_ = 0;
    bool started_ = false;
    double fraction_ = 0;
};

} // namespace mentat
