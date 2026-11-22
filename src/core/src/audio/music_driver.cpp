// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/audio/music_driver.hpp"

#include <algorithm>
#include <utility>

namespace mentat {

namespace {

constexpr std::uint16_t kStepsPerMeasure = 96;
constexpr std::uint16_t kForever = 0xFFFF;

// Instrument bytes (see docs/formats/music.md).
constexpr std::size_t kFirstOp = 2;
constexpr std::size_t kSecondOp = 15;
constexpr std::size_t kWave = 28;
constexpr std::size_t kVelocity = 30;
constexpr std::size_t kVelocityFeedback = 32;
constexpr std::size_t kFineBend = 33;
constexpr std::size_t kKeyShift = 34;
constexpr std::size_t kSlideSteps = 35;
constexpr std::size_t kSlideAmount = 36;
constexpr std::size_t kPressure = 38;
constexpr std::size_t kPressureFeedback = 27;

// Operator bytes, in AdLib order.
enum Op : std::size_t {
    KeyScale = 0,
    Multiple = 1,
    Feedback = 2,
    Attack = 3,
    Sustain = 4,
    Sustaining = 5,
    Decay = 6,
    Release = 7,
    Level = 8,
    Tremolo = 9,
    Vibrato = 10,
    KeyRate = 11,
    Connection = 12,
};

std::uint8_t levelByte(std::uint8_t keyScale, std::uint8_t level) {
    return static_cast<std::uint8_t>(((keyScale & 3) << 6) | (level & 0x3F));
}

// The change a signed amount makes, with `bits` 4 for levels and 6 for the
// feedback. A positive amount scales `positive`, a negative one `negative`.
int amountOf(std::uint8_t amount, int positive, int negative, int bits) {
    const auto a = static_cast<std::int8_t>(amount);
    if (a == 0) {
        return 0;
    }
    if (a > 0) {
        return (positive & 0xFF) >> std::max(0, bits - a);
    }
    return (negative & 0xFF) >> std::max(0, bits + a);
}

// Velocity: a positive amount scales 128 − velocity, a negative one the
// velocity. Pressure is the other way round.
int velocityAmount(std::uint8_t amount, std::uint8_t v, int bits) { return amountOf(amount, 0x80 - v, v, bits); }
int pressureAmount(std::uint8_t amount, std::uint8_t v, int bits) { return amountOf(amount, v, 0x80 - v, bits); }

std::uint8_t feedbackWith(std::uint8_t connection, int change) {
    int v = ((change & 0xFE) + connection) & 0xFF;
    if (v > 15) {
        v = (v & 15) | 14;
    }
    return static_cast<std::uint8_t>(v);
}

// The AdLib Gold player keeps the side bits (4 and 5) of the 0xC0 value
// apart from the feedback.
std::uint8_t goldFeedbackWith(std::uint8_t connection, int change) {
    return static_cast<std::uint8_t>(feedbackWith(connection & 0x0F, change) | (connection & 0x30));
}

// A register for a channel or operator place, the top bit of `place`
// picking the second register set.
std::uint16_t regAt(int base, std::uint8_t place) {
    return static_cast<std::uint16_t>(base + (place & 0x7F) + ((place & 0x80) != 0 ? 0x100 : 0));
}

constexpr std::uint8_t kSecondSet = 0x80;
constexpr std::uint16_t kSecondSetMask = 0x8000;

} // namespace

MusicDriver::MusicDriver(const MusicTables& tables) : tables_(tables) {
    for (std::size_t v = 0; v < voices_.size(); ++v) {
        voices_[v].frequency = tables_.firstFrequency[v];
    }
}

std::uint8_t MusicDriver::mixerLevels(std::uint8_t volume, std::uint8_t balance) {
    const int v = volume >> 3;
    const int one = std::min<int>(balance, 0x78);
    const int sideOne = ((((one << 8) / 0xF0) & 0xFF) * v) >> 8;
    const int two = std::min<int>((0xF0 - balance) & 0xFF, 0x78);
    const int sideTwo = ((((two << 8) / 0xF0) & 0xFF) * v) >> 8;
    return static_cast<std::uint8_t>(((sideTwo & 0x0F) << 4) | (sideOne & 0x0F));
}

void MusicDriver::init(OplSink& opl) {
    opl.writeRegister(0x01, 0x20);
    opl.writeRegister(0xBD, 0x00);
    opl.writeRegister(0x08, 0x40);
    if (gold()) {
        opl.writeRegister(0x105, 0x01); // the chip's own (OPL3) mode
        fourOpSwitches_ = 0;
        opl.writeRegister(0x104, fourOpSwitches_);
        opl.writeControl(0x06, 0xFB); // bass
        opl.writeControl(0x07, 0xF7); // treble
        opl.writeControl(0x04, 0xF7); // volume
        opl.writeControl(0x05, 0xF7);
        opl.writeControl(0x09, 0xFF); // FM volume
        opl.writeControl(0x0A, 0xFF);
    }
    stop(opl);
}

void MusicDriver::stop(OplSink& opl) {
    for (int v = voiceCount() - 1; v >= 0; --v) {
        writeFrequency(opl, v, voices_[static_cast<std::size_t>(v)].frequency);
    }
    status_.flags = 0;
}

void MusicDriver::setVolume(OplSink&, std::uint8_t volume, std::uint8_t balance) {
    // Takes effect for the next piece, and the playing one moves to it one
    // step a tick.
    volume_ = target_ = mixerLevels(volume, balance);
    fadePattern_ = 0xFFFF;
}

void MusicDriver::fade(int ticks, std::uint8_t volume, std::uint8_t balance) {
    target_ = mixerLevels(volume, balance);
    if (ticks < 0x60) {
        fadePattern_ = 0xFFFF;
    } else if (ticks < 0xC0) {
        fadePattern_ = 0xAAAA;
    } else if (ticks < 0x180) {
        fadePattern_ = 0x8888;
    } else if (ticks < 0x300) {
        fadePattern_ = 0x8080;
    } else {
        fadePattern_ = 0x8000;
    }
    if (status_.playing()) {
        status_.flags |= MusicStatus::kFading;
    }
}

void MusicDriver::start(OplSink& opl, std::shared_ptr<const Song> song, int plays) {
    song_ = std::move(song);
    plays_ = plays;
    // The fastest release on every operator.
    if (gold()) {
        for (int op = 0x15; op >= 0; --op) {
            if ((op & 7) == 6 || (op & 7) == 7) {
                continue;
            }
            opl.writeRegister(static_cast<std::uint16_t>(0x80 + op), 0xFF);
            opl.writeRegister(static_cast<std::uint16_t>(0x180 + op), 0xFF);
        }
        // The piece's stereo mode and surround settings.
        opl.writeControl(0x08, song_->byteOr0(Song::kGoldSettings));
        for (std::uint8_t r = 0; r < Song::kGoldSettingsSize - 1; ++r) {
            opl.writeSurround(r, song_->byteOr0(Song::kGoldSettings + 1 + r));
        }
    } else {
        for (int v = 0; v < voiceCount(); ++v) {
            opl.writeRegister(regAt(0x80, tables_.firstOperator[static_cast<std::size_t>(v)]), 0xFF);
        }
        for (int v = 0; v < voiceCount(); ++v) {
            opl.writeRegister(regAt(0x80, tables_.secondOperator[static_cast<std::size_t>(v)]), 0xFF);
        }
    }
    for (int v = 0; v < voiceCount(); ++v) {
        auto& voice = voices_[static_cast<std::size_t>(v)];
        voice.start = song_->voiceStart(v);
        voice.instrument = 0xFF;
        voice.note = 0;
        voice.slideLeft = 0;
        voice.slideSteps = 0;
    }
    resetVoices();
    current_ = volume_;
    target_ = current_;
    writeMixer(opl);
    counter_ = 0;
    repeatsLeft_ = 0;
    status_.flags = MusicStatus::kPlaying;
    step(opl);
}

void MusicDriver::resetVoices() {
    status_.measure = 1;
    status_.countdown = kStepsPerMeasure;
    for (int v = 0; v < voiceCount(); ++v) {
        auto& voice = voices_[static_cast<std::size_t>(v)];
        voice.at = voice.start;
        voice.wait = kForever;
        voice.channels = 0;
        if (voice.at != 0) {
            voice.wait = static_cast<std::uint16_t>(readWait(voice) + 1);
        }
    }
    used_[0] = used_[1] = 0;
}

std::uint8_t MusicDriver::byteAt(std::size_t at) const {
    const auto& bytes = song_->bytes();
    // A list that runs off the end reads as its end.
    return at < bytes.size() ? bytes[at] : 0xF0;
}

std::uint16_t MusicDriver::readWait(Voice& voice) {
    std::uint32_t value = 0;
    std::uint8_t b = 0;
    int count = 0;
    do {
        b = byteAt(voice.at++);
        value = (value << 7) | (b & 0x7F);
        ++count;
    } while ((b & 0x80) != 0 && count < 8);
    return value > 0xFFFF ? 0xFFFF : static_cast<std::uint16_t>(value);
}

void MusicDriver::tick(OplSink& opl) {
    if (status_.playing()) {
        const auto high = static_cast<std::uint8_t>(counter_ >> 8);
        counter_ = static_cast<std::uint16_t>((counter_ & 0xFF) | (static_cast<std::uint8_t>(high - 1) << 8));
        if ((counter_ & 0x8000) != 0) {
            step(opl);
        }
        const bool carry = (fadePattern_ & 0x8000) != 0;
        fadePattern_ = static_cast<std::uint16_t>((fadePattern_ << 1) | (carry ? 1 : 0));
        if (carry) {
            fadeStep(opl);
        }
    }
}

void MusicDriver::fadeStep(OplSink& opl) {
    if (current_ == target_) {
        fadePattern_ = 1;
        status_.flags &= static_cast<std::uint8_t>(~MusicStatus::kFading);
        return;
    }
    int value = current_;
    if ((current_ & 0x0F) != (target_ & 0x0F)) {
        value += (current_ & 0x0F) < (target_ & 0x0F) ? 1 : -1;
    }
    if ((value & 0xF0) != (target_ & 0xF0)) {
        value += (value & 0xF0) < (target_ & 0xF0) ? 0x10 : -0x10;
    }
    current_ = static_cast<std::uint8_t>(value);
    if (current_ == 0) {
        for (int v = voiceCount() - 1; v >= 0; --v) {
            writeFrequency(opl, v, voices_[static_cast<std::size_t>(v)].frequency);
        }
        status_.flags = 0;
    }
    writeMixer(opl);
}

void MusicDriver::writeMixer(OplSink& opl) {
    // The Sound Blaster Pro's mixer is not a chip register here: the sound
    // takes its levels from mixer(). The AdLib Gold's FM volumes are.
    if (gold()) {
        opl.writeControl(0x09, static_cast<std::uint8_t>(((current_ >> 4) << 1) | 0x81));
        opl.writeControl(0x0A, static_cast<std::uint8_t>(((current_ & 0x0F) << 1) | 0x81));
    }
}

void MusicDriver::repeatCheck() {
    if (repeatsLeft_ == 0) {
        if (status_.measure == song_->repeatStart() && status_.countdown == kStepsPerMeasure) {
            for (std::size_t v = 0; v < voices_.size(); ++v) {
                saved_[v] = Saved{voices_[v].wait, voices_[v].at};
            }
            repeatsLeft_ = static_cast<std::uint16_t>(song_->repeatCount() - 1);
        }
    } else if (status_.measure == song_->repeatEnd()) {
        --repeatsLeft_;
        for (std::size_t v = 0; v < voices_.size(); ++v) {
            voices_[v].wait = saved_[v].wait;
            voices_[v].at = saved_[v].at;
        }
        status_.measure = song_->repeatStart();
    }
}

void MusicDriver::step(OplSink& opl) {
    counter_ = static_cast<std::uint16_t>(counter_ + song_->tempo());
    repeatCheck();
    for (int v = 0; v < voiceCount(); ++v) {
        auto& voice = voices_[static_cast<std::size_t>(v)];
        if (--voice.wait == 0) {
            while (voice.at != 0 && status_.playing()) {
                runEvent(opl, v);
                if (voice.wait != 0) {
                    break;
                }
            }
            if (!status_.playing()) {
                return;
            }
        } else if (voice.slideLeft > 0 && voice.at != 0) {
            --voice.slideLeft;
            voice.bend = static_cast<std::uint8_t>(voice.bend + voice.slideAmount);
            bendPitch(opl, v, voice.bend);
        }
    }
    if (--status_.countdown == 0) {
        status_.countdown = kStepsPerMeasure;
        ++status_.measure;
    }
}

void MusicDriver::runEvent(OplSink& opl, int index) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const std::uint8_t status = byteAt(voice.at);
    const std::uint8_t data = byteAt(voice.at + 1);
    voice.at += 2;
    switch ((status >> 4) & 7) {
    case 0: // note off
        ++voice.at;
        voice.wait = readWait(voice);
        noteOff(opl, index, data);
        break;
    case 1: { // note on
        const std::uint8_t velocity = byteAt(voice.at++);
        voice.wait = readWait(voice);
        noteOn(opl, index, data, velocity);
        break;
    }
    case 2:
    case 3:
        voice.wait = readWait(voice);
        break;
    case 4:
        voice.wait = readWait(voice);
        instrument(opl, index, data);
        break;
    case 5:
        voice.wait = readWait(voice);
        pressure(opl, index, data);
        break;
    case 6:
        voice.wait = readWait(voice);
        // The bend is not kept: an automatic slide goes on from its own.
        bendPitch(opl, index, data);
        break;
    default: // end of the list
        voice.wait = kForever;
        voice.at -= 2;
        if (index != 0) {
            if (gold()) {
                freeChannels(index);
            }
            break;
        }
        if (plays_ != 0 && --plays_ == 0) {
            for (auto& v : voices_) {
                v.wait = kForever;
            }
            stop(opl);
            break;
        }
        resetVoices();
        repeatCheck();
        --voices_[0].wait;
        break;
    }
}

std::uint8_t MusicDriver::operatorOf(int index, int k) const {
    const auto i = static_cast<std::size_t>(index);
    const std::uint8_t op = (k & 1) == 0 ? tables_.firstOperator[i] : tables_.secondOperator[i];
    return static_cast<std::uint8_t>(k < 2 ? op : op + 8);
}

void MusicDriver::noteOff(OplSink& opl, int index, std::uint8_t key) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const auto note = static_cast<std::uint8_t>(key + voice.keyShift);
    if (voice.note == note) {
        voice.note = 0;
        if (gold()) {
            releaseAfterNote(index);
        }
        writeFrequency(opl, index, voice.frequency);
    }
}

void MusicDriver::releaseAfterNote(int index) {
    // A voice that rests a while before a new instrument or its end gives
    // its channels back.
    const auto& voice = voices_[static_cast<std::size_t>(index)];
    if (voice.wait < 0x30) {
        return;
    }
    const std::uint8_t next = byteAt(voice.at);
    if (next == 0xFF || (next & 0xF0) == 0xC0) {
        freeChannels(index);
    }
}

void MusicDriver::noteOn(OplSink& opl, int index, std::uint8_t key, std::uint8_t velocity) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const int ops = voice.fourOps ? 4 : 2;
    for (int k = 0; k < ops; ++k) {
        std::uint8_t level = voice.baseLevel[k];
        if (voice.velocityAmount[k] != 0) {
            const int change = velocityAmount(voice.velocityAmount[k], velocity, 4);
            const int v = std::min(0x3F, (level & 0x3F) + change);
            level = static_cast<std::uint8_t>((level & 0xC0) | v);
            opl.writeRegister(regAt(0x40, operatorOf(index, k)), level);
        }
        voice.currentLevel[k] = level;
    }
    if (voice.feedbackAmount == 0) {
        voice.currentConnection = voice.baseConnection;
    } else {
        const int change = velocityAmount(voice.feedbackAmount, velocity, 6);
        voice.currentConnection = gold() ? goldFeedbackWith(voice.baseConnection, change)
                                         : feedbackWith(voice.baseConnection, change);
        opl.writeRegister(regAt(0xC0, tables_.channel[static_cast<std::size_t>(index)]), voice.currentConnection);
    }
    if (voice.note != 0) {
        writeFrequency(opl, index, 0);
    }
    voice.note = static_cast<std::uint8_t>(key + voice.keyShift);
    voice.slideLeft = voice.slideSteps;
    voice.bend = 0x40;
    int n = voice.note - 24;
    if (n < 0 || n >= 96) {
        n = 0;
    }
    voice.frequency = static_cast<std::uint16_t>(tables_.frequency[static_cast<std::size_t>(n % 12)] |
                                                 ((n / 12) << 10));
    writeFrequency(opl, index, static_cast<std::uint16_t>(voice.frequency | 0x2000));
}

void MusicDriver::instrument(OplSink& opl, int index, std::uint8_t number) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    // The Sound Blaster Pro player skips an instrument the voice already
    // has; the AdLib Gold player sets it again.
    if (!gold() && voice.instrument == number) {
        return;
    }
    voice.instrument = number;
    const auto ins = song_->instrument(number);
    const auto* first = ins.data() + kFirstOp;
    const auto* second = ins.data() + kSecondOp;
    voice.fourOps = gold() && ins[0] == 4;
    if (gold()) {
        takeChannels(index, voice.fourOps);
    }
    voice.fineBend = ins[kFineBend];
    voice.keyShift = ins[kKeyShift];
    voice.baseLevel[0] = levelByte(first[KeyScale], first[Level]);
    voice.baseLevel[1] = levelByte(second[KeyScale], second[Level]);
    voice.velocityAmount[0] = ins[kVelocity];
    voice.velocityAmount[1] = ins[kVelocity + 1];
    voice.pressureAmount[0] = ins[kPressure];
    voice.pressureAmount[1] = ins[kPressure + 1];
    if (gold()) {
        // The second operator's feedback byte holds the sides it plays on.
        voice.baseConnection = static_cast<std::uint8_t>(
            (((first[Feedback] | (second[Feedback] << 3)) << 1) | ((first[Connection] & 1) ^ 1)) & 0xFF);
    } else {
        voice.baseConnection =
            static_cast<std::uint8_t>(((first[Feedback] << 1) | ((first[Connection] & 1) ^ 1)) & 0xFF);
    }
    // Pressure before the first note works from the instrument's value.
    voice.currentConnection = voice.baseConnection;
    voice.feedbackAmount = ins[kVelocityFeedback];
    voice.pressureFeedbackAmount = ins[kPressureFeedback];
    voice.slideSteps = ins[kSlideSteps];
    voice.slideAmount = ins[kSlideAmount];
    voice.slideLeft = 0;

    // A four-operator sound takes operators 3 and 4 from the next
    // instrument (zeros past the last one).
    std::array<std::uint8_t, Song::kInstrumentSize> next{};
    if (voice.fourOps) {
        const std::size_t at = static_cast<std::size_t>(ins.data() - song_->bytes().data()) + Song::kInstrumentSize;
        for (std::size_t b = 0; b < next.size(); ++b) {
            next[b] = song_->byteOr0(at + b);
        }
        voice.baseLevel[2] = levelByte(next[kFirstOp + KeyScale], next[kFirstOp + Level]);
        voice.baseLevel[3] = levelByte(next[kSecondOp + KeyScale], next[kSecondOp + Level]);
        voice.velocityAmount[2] = next[kVelocity];
        voice.velocityAmount[3] = next[kVelocity + 1];
        voice.pressureAmount[2] = next[kPressure];
        voice.pressureAmount[3] = next[kPressure + 1];
    }

    const auto i = static_cast<std::size_t>(index);
    writeOperators(opl, ins, tables_.channel[i], tables_.firstOperator[i], tables_.secondOperator[i]);
    if (!gold()) {
        return;
    }
    const std::uint8_t op = tables_.firstOperator[i];
    if ((op & 0x10) != 0) {
        return; // channels 6 to 8 have no four-operator switch
    }
    const int bit = (op & 3) + ((op & kSecondSet) != 0 ? 3 : 0);
    if (voice.fourOps) {
        fourOpSwitches_ = static_cast<std::uint8_t>(fourOpSwitches_ | (1 << bit));
        opl.writeRegister(0x104, fourOpSwitches_);
        writeOperators(opl, next, tables_.pairedChannel[i], static_cast<std::uint8_t>(op + 8),
                       static_cast<std::uint8_t>(tables_.secondOperator[i] + 8));
    } else {
        fourOpSwitches_ = static_cast<std::uint8_t>(fourOpSwitches_ & ~(1 << bit));
        opl.writeRegister(0x104, fourOpSwitches_);
    }
}

void MusicDriver::writeOperators(OplSink& opl, std::span<const std::uint8_t> ins, std::uint8_t channel,
                                 std::uint8_t firstOp, std::uint8_t secondOp) {
    const auto* first = ins.data() + kFirstOp;
    std::uint8_t connection =
        static_cast<std::uint8_t>(((first[Feedback] << 1) | ((first[Connection] & 1) ^ 1)) & 0x0F);
    int waveMask = 3;
    if (gold()) {
        connection = static_cast<std::uint8_t>(connection | ((ins[kSecondOp + Feedback] << 4) & 0x30));
        waveMask = 7;
    }
    opl.writeRegister(regAt(0xC0, channel), connection);
    const std::uint8_t ops[2] = {firstOp, secondOp};
    for (int k = 0; k < 2; ++k) {
        const auto* p = ins.data() + (k == 0 ? kFirstOp : kSecondOp);
        const auto op = ops[k];
        opl.writeRegister(regAt(0xE0, op), static_cast<std::uint8_t>(ins[kWave + static_cast<std::size_t>(k)] & waveMask));
        opl.writeRegister(regAt(0x40, op), levelByte(p[KeyScale], p[Level]));
        opl.writeRegister(regAt(0x60, op), static_cast<std::uint8_t>((p[Attack] << 4) | (p[Decay] & 0x0F)));
        opl.writeRegister(regAt(0x80, op), static_cast<std::uint8_t>((p[Sustain] << 4) | (p[Release] & 0x0F)));
        opl.writeRegister(regAt(0x20, op),
                          static_cast<std::uint8_t>(((p[Tremolo] & 1) << 7) | ((p[Vibrato] & 1) << 6) |
                                                    ((p[Sustaining] & 1) << 5) | ((p[KeyRate] & 1) << 4) |
                                                    (p[Multiple] & 0x0F)));
    }
}

void MusicDriver::takeChannels(int index, bool fourOps) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    std::uint16_t used[2] = {used_[0], used_[1]};
    // What the voice held is free again.
    const auto held = static_cast<std::uint16_t>(~voice.channels);
    if ((held & kSecondSetMask) != 0) {
        used[0] &= held;
    } else {
        used[1] &= held;
    }

    int set = -1;
    int channel = 0;
    bool foundPair = false;
    const auto take = [&](int s, int c) {
        set = s;
        channel = c;
    };
    const auto isFree = [&](int s, int c) { return (used[s] & (1 << c)) == 0; };
    if (fourOps) {
        // A pair of channels c and c + 3, first set first.
        for (int s = 0; s < 2 && set < 0; ++s) {
            for (int c = 0; c < 3 && set < 0; ++c) {
                if (isFree(s, c) && isFree(s, c + 3)) {
                    take(s, c);
                    foundPair = true;
                }
            }
        }
    }
    const auto highestFree = [&](int s) {
        for (int c = 8; c >= 6; --c) {
            if (isFree(s, c)) {
                return c;
            }
        }
        return -1;
    };
    // In a pair: the lower channel when it is free, otherwise the upper.
    const auto inPair = [&](int s, int pair) { take(s, isFree(s, pair) ? pair : pair + 3); };
    const auto halfUsed = [&](int s) {
        return ((used[s] & 7) ^ ((used[s] >> 3) & 7)) & 7;
    };
    const auto highestPairWithRoom = [&](int s) {
        const auto free = static_cast<std::uint16_t>(~used[s]);
        if ((free & 0x24) != 0) {
            return 2;
        }
        return (free & 0x12) != 0 ? 1 : 0;
    };
    if (set < 0) {
        if (const int c = highestFree(0); c >= 0) {
            take(0, c);
        } else if (const int c1 = highestFree(1); c1 >= 0) {
            take(1, c1);
        } else if (const int half1 = halfUsed(1); half1 != 0) {
            // A pair that already has one two-operator sound, lowest first.
            inPair(1, (half1 & 1) != 0 ? 0 : ((half1 & 2) != 0 ? 1 : 2));
        } else if (const int half0 = halfUsed(0); half0 != 0) {
            inPair(0, (half0 & 1) != 0 ? 0 : ((half0 & 2) != 0 ? 1 : 2));
        } else if ((~used[1] & 0x3F) != 0) {
            inPair(1, highestPairWithRoom(1));
        } else {
            // Everything is taken: a channel of the first set is shared.
            inPair(0, highestPairWithRoom(0));
        }
    }

    const auto bits = static_cast<std::uint16_t>((1 << channel) | (foundPair ? 1 << (channel + 3) : 0));
    used[set] = static_cast<std::uint16_t>(used[set] | bits);
    voice.channels = static_cast<std::uint16_t>(bits | (set == 1 ? kSecondSetMask : 0));
    used_[0] = used[0];
    used_[1] = used[1];

    const auto bank = static_cast<std::uint8_t>(set == 1 ? kSecondSet : 0);
    const auto op = static_cast<std::uint8_t>((channel / 3) * 8 + channel % 3);
    const auto i = static_cast<std::size_t>(index);
    tables_.channel[i] = static_cast<std::uint8_t>(bank | channel);
    tables_.pairedChannel[i] = static_cast<std::uint8_t>(bank | (channel + 3));
    tables_.firstOperator[i] = static_cast<std::uint8_t>(bank | op);
    tables_.secondOperator[i] = static_cast<std::uint8_t>(bank | (op + 3));
}

void MusicDriver::freeChannels(int index) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const auto free = static_cast<std::uint16_t>(~voice.channels);
    voice.channels = 0;
    if ((free & kSecondSetMask) != 0) {
        used_[0] &= free;
    } else {
        used_[1] &= free;
    }
}

void MusicDriver::pressure(OplSink& opl, int index, std::uint8_t value) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const int ops = voice.fourOps ? 4 : 2;
    for (int k = 0; k < ops; ++k) {
        if (voice.pressureAmount[k] == 0) {
            continue;
        }
        const std::uint8_t level = voice.currentLevel[k];
        const int change = pressureAmount(voice.pressureAmount[k], value, 4);
        const int v = std::max(0, (level & 0x3F) - change);
        opl.writeRegister(regAt(0x40, operatorOf(index, k)), static_cast<std::uint8_t>((level & 0xC0) | v));
    }
    if (voice.pressureFeedbackAmount != 0) {
        const int change = pressureAmount(voice.pressureFeedbackAmount, value, 6);
        opl.writeRegister(regAt(0xC0, tables_.channel[static_cast<std::size_t>(index)]),
                          gold() ? goldFeedbackWith(voice.currentConnection, change)
                                 : feedbackWith(voice.currentConnection, change));
    }
}
void MusicDriver::bendPitch(OplSink& opl, int index, std::uint8_t bend) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    if (voice.note == 0) {
        return;
    }
    const int n = static_cast<std::uint8_t>(voice.note - 24);
    int octave = n / 12;
    int semitone = n % 12;
    int frequency = 0;
    const auto down = [&](int whole) {
        semitone -= whole;
        while (semitone < 0) {
            semitone += 12;
            if (--octave < 0) {
                octave = 0;
                semitone = 0;
            }
        }
    };
    const auto up = [&](int whole) {
        semitone += whole;
        while (semitone >= 12) {
            semitone -= 12;
            ++octave;
        }
    };
    const auto base = [&] { return static_cast<int>(tables_.frequency[static_cast<std::size_t>(semitone)]); };
    const auto fine = [&](int rest) {
        const auto& table = semitone < 6 ? tables_.fineLow : tables_.fineHigh;
        return static_cast<int>(table[static_cast<std::size_t>(rest)]);
    };
    if (voice.fineBend == 0) {
        if (bend < 0x40) {
            const int steps = 0x40 - bend;
            down(steps >> 5);
            const int rest = (steps & 31) << 3;
            frequency = base() - ((tables_.distance[static_cast<std::size_t>(semitone)] * rest) >> 8);
        } else {
            const int steps = bend - 0x40 + 1;
            up(steps >> 5);
            const int rest = (steps & 31) << 3;
            frequency = base() + ((tables_.distance[static_cast<std::size_t>(semitone) + 1] * rest) >> 8);
        }
    } else if (bend < 0x40) {
        const int steps = 0x40 - bend;
        down(steps / 5);
        frequency = base() - fine(steps % 5);
    } else {
        const int steps = bend - 0x40;
        up(steps / 5);
        frequency = base() + fine(steps % 5);
    }
    voice.frequency = static_cast<std::uint16_t>((frequency & 0xFFFF) | ((octave << 10) & 0xFFFF));
    writeFrequency(opl, index, static_cast<std::uint16_t>(voice.frequency | 0x2000));
}

void MusicDriver::writeFrequency(OplSink& opl, int index, std::uint16_t value) {
    const std::uint8_t channel = tables_.channel[static_cast<std::size_t>(index)];
    opl.writeRegister(regAt(0xA0, channel), static_cast<std::uint8_t>(value & 0xFF));
    opl.writeRegister(regAt(0xB0, channel), static_cast<std::uint8_t>(value >> 8));
}

} // namespace mentat
