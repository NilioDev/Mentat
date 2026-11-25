// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/audio/midi_driver.hpp"

#include <utility>

namespace mentat {

namespace {

constexpr std::uint16_t kStepsPerMeasure = 96;
constexpr std::uint16_t kForever = 0xFFFF;
constexpr std::uint8_t kStartVolume = 0x60;

std::uint8_t scaled(std::uint8_t value, std::uint8_t volume) {
    return static_cast<std::uint8_t>((value * volume) >> 8);
}

} // namespace

void MidiMusicDriver::init(MidiSink& midi) { stop(midi); }

void MidiMusicDriver::silence(MidiSink& midi) {
    // Every note off, and the pitch bend back to the middle, on the ten
    // channels.
    for (int c = 0; c < kChannels; ++c) {
        const std::uint8_t notesOff[] = {static_cast<std::uint8_t>(0xB0 | c), 0x7B, 0x00};
        midi.send(notesOff);
        const std::uint8_t bend[] = {static_cast<std::uint8_t>(0xE0 | c), 0x00, 0x40};
        midi.send(bend);
    }
}

void MidiMusicDriver::stop(MidiSink& midi) {
    silence(midi);
    status_.flags = 0;
}

void MidiMusicDriver::setVolume(std::uint8_t volume) {
    volume_ = target_ = volume;
    fadePattern_ = 0xFFFF;
}

void MidiMusicDriver::fade(int ticks, std::uint8_t volume) {
    target_ = volume;
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

void MidiMusicDriver::start(MidiSink& midi, std::shared_ptr<const Song> song, int plays) {
    song_ = std::move(song);
    plays_ = plays;
    silence(midi);
    listed_ = 0;
    for (int v = 0; v < kVoices; ++v) {
        voices_[static_cast<std::size_t>(v)].start = song_->voiceStart(v);
        if (song_->voiceStart(v) != 0) {
            ++listed_;
        }
    }
    for (int c = 0; c < kVoices; ++c) {
        channelVolume_[static_cast<std::size_t>(c)] = kStartVolume;
    }
    resetVoices();
    current_ = target_ = volume_;
    counter_ = 0;
    repeatsLeft_ = 0;
    step(midi);
    status_.flags = MusicStatus::kPlaying;
}

void MidiMusicDriver::resetVoices() {
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

std::uint8_t MidiMusicDriver::byteAt(std::size_t at) const {
    const auto& bytes = song_->bytes();
    // A list that runs off the end reads as its end.
    return at < bytes.size() ? bytes[at] : 0xF0;
}

std::uint16_t MidiMusicDriver::readWait(Voice& voice) {
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

void MidiMusicDriver::tick(MidiSink& midi) {
    if (!status_.playing()) {
        return;
    }
    const auto high = static_cast<std::uint8_t>(counter_ >> 8);
    counter_ = static_cast<std::uint16_t>((counter_ & 0xFF) | (static_cast<std::uint8_t>(high - 1) << 8));
    if ((counter_ & 0x8000) != 0) {
        step(midi);
    }
    const bool carry = (fadePattern_ & 0x8000) != 0;
    fadePattern_ = static_cast<std::uint16_t>((fadePattern_ << 1) | (carry ? 1 : 0));
    if (carry) {
        fadeStep(midi);
    }
}

void MidiMusicDriver::fadeStep(MidiSink& midi) {
    if (current_ == target_) {
        fadePattern_ = 1;
        status_.flags &= static_cast<std::uint8_t>(~MusicStatus::kFading);
        return;
    }
    // At most three a step either way.
    int change = current_ - target_;
    change = change > 3 ? 3 : (change < -3 ? -3 : change);
    current_ = static_cast<std::uint8_t>(current_ - change);
    for (int c = 0; c < kChannels; ++c) {
        const std::uint8_t message[] = {static_cast<std::uint8_t>(0xB0 | c), 0x07,
                                        scaled(channelVolume_[static_cast<std::size_t>(c)], current_)};
        midi.send(message);
    }
    if (current_ == 0) {
        status_.flags = 0;
    }
}

void MidiMusicDriver::repeatCheck() {
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

void MidiMusicDriver::step(MidiSink& midi) {
    counter_ = static_cast<std::uint16_t>(counter_ + song_->tempo());
    repeatCheck();
    for (int v = 0; v < listed_; ++v) {
        auto& voice = voices_[static_cast<std::size_t>(v)];
        if (--voice.wait == 0) {
            // An event that waits 0 lets the next one follow at once.
            while (voice.at != 0) {
                runEvent(midi, v);
                if (voice.wait != 0) {
                    break;
                }
            }
        }
    }
    if (--status_.countdown == 0) {
        status_.countdown = kStepsPerMeasure;
        ++status_.measure;
    }
}

void MidiMusicDriver::runEvent(MidiSink& midi, int index) {
    auto& voice = voices_[static_cast<std::size_t>(index)];
    const std::uint8_t status = byteAt(voice.at);
    const std::uint8_t data = byteAt(voice.at + 1);
    voice.at += 2;
    switch ((status >> 4) & 7) {
    case 0:   // note off
    case 1:   // note on
    case 6: { // pitch bend
        const std::uint8_t last = byteAt(voice.at++);
        voice.wait = readWait(voice);
        const std::uint8_t message[] = {status, data, last};
        midi.send(message);
        break;
    }
    case 2: // key pressure: left out
        voice.wait = readWait(voice);
        break;
    case 3: { // controller; the volume is kept and scaled by the game's
        std::uint8_t value = byteAt(voice.at++);
        voice.wait = readWait(voice);
        if (data == 7) {
            const auto channel = static_cast<std::size_t>(status & 0x0F);
            if (channel < channelVolume_.size()) {
                channelVolume_[channel] = value;
            }
            value = scaled(value, current_);
        }
        const std::uint8_t message[] = {status, data, value};
        midi.send(message);
        break;
    }
    case 4:   // program change
    case 5: { // channel pressure
        voice.wait = readWait(voice);
        const std::uint8_t message[] = {status, data};
        midi.send(message);
        break;
    }
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
            stop(midi);
            break;
        }
        resetVoices();
        repeatCheck();
        --voices_[0].wait;
        break;
    }
}

} // namespace mentat
