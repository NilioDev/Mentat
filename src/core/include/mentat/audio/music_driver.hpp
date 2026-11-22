// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plays the original's FM music the way its music players do, as register
// writes for an FM chip: the Sound Blaster Pro player (docs/rules/music.md)
// or the AdLib Gold player (docs/rules/music-gold.md).
#pragma once

#include "mentat/audio/music.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>

namespace mentat {

class OplSink {
public:
    virtual ~OplSink() = default;
    // `reg` from 0x100 up is the chip's second register set.
    virtual void writeRegister(std::uint16_t reg, std::uint8_t value) = 0;
    // The AdLib Gold's control chip (volumes, tone, stereo mode).
    virtual void writeControl(std::uint8_t, std::uint8_t) {}
    // The AdLib Gold's surround module (sent through the control chip).
    virtual void writeSurround(std::uint8_t, std::uint8_t) {}
};

struct MusicStatus {
    static constexpr std::uint8_t kPlaying = 0x80;
    static constexpr std::uint8_t kFading = 0x40;
    std::uint8_t flags = 0;
    std::uint16_t measure = 1;
    std::uint16_t countdown = 96;

    bool playing() const { return (flags & kPlaying) != 0; }
    // The position the game waits for: 16 per measure.
    std::uint16_t position() const {
        return static_cast<std::uint16_t>((measure << 4) | (((96 - countdown) / 6) & 0x0F));
    }
};

class MusicDriver {
public:
    explicit MusicDriver(const MusicTables& tables);

    MusicCard card() const { return tables_.card; }

    // Sets the chip up, as the player does when the game loads it.
    void init(OplSink& opl);
    // Starts a piece. `plays` 0 repeats it forever.
    void start(OplSink& opl, std::shared_ptr<const Song> song, int plays);
    void stop(OplSink& opl);
    // Lets the piece end after the play that is running.
    void endAfterThisPlay() { plays_ = 1; }
    // The game's volume byte and balance (0x78 is the middle).
    void setVolume(OplSink& opl, std::uint8_t volume, std::uint8_t balance);
    void fade(int ticks, std::uint8_t volume, std::uint8_t balance);
    // One timer tick.
    void tick(OplSink& opl);

    const MusicStatus& status() const { return status_; }
    // The volume levels: high nibble one side, low nibble the other.
    std::uint8_t mixer() const { return current_; }

    static std::uint8_t mixerLevels(std::uint8_t volume, std::uint8_t balance);

private:
    struct Voice {
        std::uint16_t wait = 0xFFFF;
        std::size_t at = 0;    // place in the song's bytes, 0 = no list
        std::size_t start = 0; // start of the list
        std::uint8_t instrument = 0xFF;
        std::uint8_t note = 0;
        std::uint8_t fineBend = 0;  // instrument byte 33
        std::uint8_t keyShift = 0;  // instrument byte 34
        std::uint8_t slideLeft = 0; // automatic slide steps left
        std::uint8_t slideSteps = 0;
        std::uint8_t bend = 0x40;
        std::uint8_t slideAmount = 0;
        bool fourOps = false;
        // Operators 1 and 2, and with four operators 3 and 4.
        std::uint8_t velocityAmount[4] = {}; // signed amounts
        std::uint8_t pressureAmount[4] = {};
        std::uint8_t baseLevel[4] = {};    // key scale and output level
        std::uint8_t currentLevel[4] = {}; // after the last note's velocity
        std::uint8_t feedbackAmount = 0;
        std::uint8_t pressureFeedbackAmount = 0;
        std::uint8_t baseConnection = 0;    // the 0xC0 value
        std::uint8_t currentConnection = 0; // after the last note's velocity
        std::uint16_t frequency = 0;        // A0/B0 without the key bit
        std::uint16_t channels = 0;         // AdLib Gold: the channels it holds, top bit for the second set
    };

    struct Saved {
        std::uint16_t wait;
        std::size_t at;
    };

    bool gold() const { return tables_.card == MusicCard::AdLibGold; }
    int voiceCount() const { return tables_.voices(); }
    void step(OplSink& opl);
    void repeatCheck();
    void resetVoices();
    std::uint8_t byteAt(std::size_t at) const;
    std::uint16_t readWait(Voice& voice);
    void runEvent(OplSink& opl, int index);
    void noteOn(OplSink& opl, int index, std::uint8_t key, std::uint8_t velocity);
    void noteOff(OplSink& opl, int index, std::uint8_t key);
    void instrument(OplSink& opl, int index, std::uint8_t number);
    void writeOperators(OplSink& opl, std::span<const std::uint8_t> ins, std::uint8_t channel,
                        std::uint8_t first, std::uint8_t second);
    void pressure(OplSink& opl, int index, std::uint8_t value);
    void bendPitch(OplSink& opl, int index, std::uint8_t bend);
    void writeFrequency(OplSink& opl, int index, std::uint16_t value);
    void writeMixer(OplSink& opl);
    void fadeStep(OplSink& opl);
    // AdLib Gold: channels for a voice and giving them back.
    void takeChannels(int index, bool fourOps);
    void freeChannels(int index);
    void releaseAfterNote(int index);
    std::uint8_t operatorOf(int index, int k) const;

    MusicTables tables_;
    std::shared_ptr<const Song> song_;
    std::array<Voice, Song::kVoices> voices_{};
    std::array<Saved, Song::kVoices> saved_{};
    MusicStatus status_;
    std::uint16_t counter_ = 0;
    std::uint16_t repeatsLeft_ = 0;
    int plays_ = 0;
    std::uint8_t volume_ = 0xEE; // levels of the volume setting
    std::uint8_t current_ = 0xEE;
    std::uint8_t target_ = 0xEE;
    std::uint16_t fadePattern_ = 0xFFFF;
    // AdLib Gold: channels in use in each register set, and the
    // four-operator switches (register 0x104).
    std::uint16_t used_[2] = {};
    std::uint8_t fourOpSwitches_ = 0;
};

} // namespace mentat
