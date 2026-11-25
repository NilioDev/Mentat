// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plays the original's Roland MT-32 music (the .M32 files) the way its MIDI
// music player (DNMID.HSQ) does, as MIDI messages for the synthesizer. See
// docs/rules/music-mt32.md.
#pragma once

#include "mentat/audio/music.hpp"
#include "mentat/audio/music_driver.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace mentat {

class MidiSink {
public:
    virtual ~MidiSink() = default;
    // One channel message: a status byte and one or two data bytes.
    virtual void send(std::span<const std::uint8_t> message) = 0;
};

class MidiMusicDriver {
public:
    // Voice lists the player reads.
    static constexpr int kVoices = 9;
    // MIDI channels whose volume it keeps and fades (1 to 10).
    static constexpr int kChannels = 10;

    // Silences the synthesizer, as the player does when the game loads it.
    void init(MidiSink& midi);
    // Starts a piece. `plays` 0 repeats it forever.
    void start(MidiSink& midi, std::shared_ptr<const Song> song, int plays);
    void stop(MidiSink& midi);
    void endAfterThisPlay() { plays_ = 1; }
    // The game's volume byte: the piece's channel volumes are scaled by it.
    void setVolume(std::uint8_t volume);
    void fade(int ticks, std::uint8_t volume);
    // One timer tick.
    void tick(MidiSink& midi);

    const MusicStatus& status() const { return status_; }
    // The volume the channels are scaled by at the moment.
    std::uint8_t volume() const { return current_; }

private:
    struct Voice {
        std::uint16_t wait = 0xFFFF;
        std::size_t at = 0;
        std::size_t start = 0;
    };
    struct Saved {
        std::uint16_t wait;
        std::size_t at;
    };

    std::uint8_t byteAt(std::size_t at) const;
    std::uint16_t readWait(Voice& voice);
    void resetVoices();
    void repeatCheck();
    void step(MidiSink& midi);
    void runEvent(MidiSink& midi, int index);
    void fadeStep(MidiSink& midi);
    void silence(MidiSink& midi);

    std::shared_ptr<const Song> song_;
    std::array<Voice, kVoices> voices_{};
    std::array<Saved, kVoices> saved_{};
    int listed_ = 0; // voices with a list: the player runs only that many
    MusicStatus status_;
    std::uint16_t counter_ = 0;
    std::uint16_t repeatsLeft_ = 0;
    int plays_ = 0;
    // The pieces' own channel volumes (controller 7). The player keeps the
    // tenth after the first nine, where its program's bytes leave 0x4D.
    std::array<std::uint8_t, kChannels> channelVolume_{0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4D};
    std::uint8_t volume_ = 0xE6; // set by the game
    std::uint8_t current_ = 0xE6;
    std::uint8_t target_ = 0xE6;
    std::uint16_t fadePattern_ = 0xFFFF;
};

} // namespace mentat
