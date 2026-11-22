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

} // namespace

MusicDriver::MusicDriver(const MusicTables& tables) : tables_(tables) {}

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
    stop(opl);
}

void MusicDriver::stop(OplSink& opl) {
    for (int v = Song::kVoices - 1; v >= 0; --v) {
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
    for (const auto op : tables_.firstOperator) {
        opl.writeRegister(static_cast<std::uint8_t>(0x80 + op), 0xFF);
    }
    for (const auto op : tables_.secondOperator) {
        opl.writeRegister(static_cast<std::uint8_t>(0x80 + op), 0xFF);
    }
    for (int v = 0; v < Song::kVoices; ++v) {
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
    writeMixer();
    counter_ = 0;
    repeatsLeft_ = 0;
    status_.flags = MusicStatus::kPlaying;
    step(opl);
}

void MusicDriver::resetVoices() {
    status_.measure = 1;
    status_.countdown = kStepsPerMeasure;
    for (auto& voice : voices_) {
        voice.at = voice.start;
        voice.wait = kForever;
        if (voice.at != 0) {
            voice.wait = static_cast<std::uint16_t>(readWait(voice) + 1);
        }
    }
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
        for (int v = Song::kVoices - 1; v >= 0; --v) {
            writeFrequency(opl, v, voices_[static_cast<std::size_t>(v)].frequency);
        }
        status_.flags = 0;
    }
    writeMixer();
}

void MusicDriver::writeMixer() {}

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
    for (int v = 0; v < Song::kVoices; ++v) {
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
            bendPitch(opl, v);
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
        voice.bend = data;
        bendPitch(opl, index);
        break;
    default: // end of the list
        voice.wait = kForever;
        voice.at -= 2;
        if (index != 0) {
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

void MusicDriver::noteOff(OplSink& opl, int index, std::uint8_t key) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const auto note = static_cast<std::uint8_t>(key + voice.keyShift);
    if (voice.note == note) {
        voice.note = 0;
        writeFrequency(opl, index, voice.frequency);
    }
}

void MusicDriver::noteOn(OplSink& opl, int index, std::uint8_t key, std::uint8_t velocity) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const auto i = static_cast<std::size_t>(index);
    const std::uint8_t ops[2] = {tables_.firstOperator[i], tables_.secondOperator[i]};
    for (int k = 0; k < 2; ++k) {
        std::uint8_t level = voice.baseLevel[k];
        if (voice.velocityAmount[k] != 0) {
            const int change = velocityAmount(voice.velocityAmount[k], velocity, 4);
            const int v = std::min(0x3F, (level & 0x3F) + change);
            level = static_cast<std::uint8_t>((level & 0xC0) | v);
            opl.writeRegister(static_cast<std::uint8_t>(0x40 + ops[k]), level);
        }
        voice.currentLevel[k] = level;
    }
    if (voice.feedbackAmount == 0) {
        voice.currentConnection = voice.baseConnection;
    } else {
        voice.currentConnection =
            feedbackWith(voice.baseConnection, velocityAmount(voice.feedbackAmount, velocity, 6));
        opl.writeRegister(static_cast<std::uint8_t>(0xC0 + index), voice.currentConnection);
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
    if (voice.instrument == number) {
        return;
    }
    voice.instrument = number;
    const auto ins = song_->instrument(number);
    const auto* first = ins.data() + kFirstOp;
    const auto* second = ins.data() + kSecondOp;
    voice.fineBend = ins[kFineBend];
    voice.keyShift = ins[kKeyShift];
    voice.baseLevel[0] = levelByte(first[KeyScale], first[Level]);
    voice.baseLevel[1] = levelByte(second[KeyScale], second[Level]);
    voice.velocityAmount[0] = ins[kVelocity];
    voice.velocityAmount[1] = ins[kVelocity + 1];
    voice.pressureAmount[0] = ins[kPressure];
    voice.pressureAmount[1] = ins[kPressure + 1];
    voice.baseConnection =
        static_cast<std::uint8_t>(((first[Feedback] << 1) | ((first[Connection] & 1) ^ 1)) & 0xFF);
    voice.feedbackAmount = ins[kVelocityFeedback];
    voice.pressureFeedbackAmount = ins[kPressureFeedback];
    voice.slideSteps = ins[kSlideSteps];
    voice.slideAmount = ins[kSlideAmount];
    voice.slideLeft = 0;

    const auto i = static_cast<std::size_t>(index);
    opl.writeRegister(static_cast<std::uint8_t>(0xC0 + index), voice.baseConnection & 0x0F);
    const std::uint8_t ops[2] = {tables_.firstOperator[i], tables_.secondOperator[i]};
    const std::uint8_t* params[2] = {first, second};
    for (int k = 0; k < 2; ++k) {
        const auto* p = params[k];
        const auto op = ops[k];
        opl.writeRegister(static_cast<std::uint8_t>(0xE0 + op), ins[kWave + static_cast<std::size_t>(k)] & 3);
        opl.writeRegister(static_cast<std::uint8_t>(0x40 + op), levelByte(p[KeyScale], p[Level]));
        opl.writeRegister(static_cast<std::uint8_t>(0x60 + op), static_cast<std::uint8_t>((p[Attack] << 4) | (p[Decay] & 0x0F)));
        opl.writeRegister(static_cast<std::uint8_t>(0x80 + op), static_cast<std::uint8_t>((p[Sustain] << 4) | (p[Release] & 0x0F)));
        opl.writeRegister(static_cast<std::uint8_t>(0x20 + op),
                          static_cast<std::uint8_t>(((p[Tremolo] & 1) << 7) | ((p[Vibrato] & 1) << 6) |
                                                    ((p[Sustaining] & 1) << 5) | ((p[KeyRate] & 1) << 4) |
                                                    (p[Multiple] & 0x0F)));
    }
}

void MusicDriver::pressure(OplSink& opl, int index, std::uint8_t value) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const auto i = static_cast<std::size_t>(index);
    const std::uint8_t ops[2] = {tables_.firstOperator[i], tables_.secondOperator[i]};
    for (int k = 0; k < 2; ++k) {
        if (voice.pressureAmount[k] == 0) {
            continue;
        }
        const std::uint8_t level = voice.currentLevel[k];
        const int change = pressureAmount(voice.pressureAmount[k], value, 4);
        const int v = std::max(0, (level & 0x3F) - change);
        opl.writeRegister(static_cast<std::uint8_t>(0x40 + ops[k]), static_cast<std::uint8_t>((level & 0xC0) | v));
    }
    if (voice.pressureFeedbackAmount != 0) {
        opl.writeRegister(static_cast<std::uint8_t>(0xC0 + index),
                          feedbackWith(voice.currentConnection, pressureAmount(voice.pressureFeedbackAmount, value, 6)));
    }
}

void MusicDriver::bendPitch(OplSink& opl, int index) {
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
        if (voice.bend < 0x40) {
            const int steps = 0x40 - voice.bend;
            down(steps >> 5);
            const int rest = (steps & 31) << 3;
            frequency = base() - ((tables_.distance[static_cast<std::size_t>(semitone)] * rest) >> 8);
        } else {
            const int steps = voice.bend - 0x40 + 1;
            up(steps >> 5);
            const int rest = (steps & 31) << 3;
            frequency = base() + ((tables_.distance[static_cast<std::size_t>(semitone) + 1] * rest) >> 8);
        }
    } else if (voice.bend < 0x40) {
        const int steps = 0x40 - voice.bend;
        down(steps / 5);
        frequency = base() - fine(steps % 5);
    } else {
        const int steps = voice.bend - 0x40;
        up(steps / 5);
        frequency = base() + fine(steps % 5);
    }
    voice.frequency = static_cast<std::uint16_t>((frequency & 0xFFFF) | ((octave << 10) & 0xFFFF));
    writeFrequency(opl, index, static_cast<std::uint16_t>(voice.frequency | 0x2000));
}

void MusicDriver::writeFrequency(OplSink& opl, int index, std::uint16_t value) {
    opl.writeRegister(static_cast<std::uint8_t>(0xA0 + index), static_cast<std::uint8_t>(value & 0xFF));
    opl.writeRegister(static_cast<std::uint8_t>(0xB0 + index), static_cast<std::uint8_t>(value >> 8));
}

} // namespace mentat
