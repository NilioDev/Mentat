// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/hnm_player.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/data/voc.hpp"
#include "mentat/engine/display.hpp"
#include "mentat/gfx/surface.hpp"

#include <utility>

namespace mentat {

namespace {

// The first sound chunk of a video starts with a VOC file header and the
// header of a sound block: rate byte at 0x1E, then 8-bit samples from 0x20.
constexpr std::size_t kFirstChunkHeader = 0x20;
constexpr std::size_t kRateByte = 0x1E;

} // namespace

HnmPlayer::HnmPlayer(HnmFile file, Options options) : file_(std::move(file)), options_(options) {}

void HnmPlayer::start(Platform& platform, Display& display) {
    display.applyPaletteBlocks(file_.headerPalette());
    next_ = 0;
    soundRate_ = 0;
    if (file_.frameCount() > 0) {
        show(platform, display, file_.frame(0), false);
    }
}

bool HnmPlayer::due(const Platform& platform, const HnmFrame& frame) const {
    if (!frame.sound.empty() && soundRate_ > 0) {
        // The original keeps two sound buffers; the next frame comes when
        // the older one has finished playing.
        return platform.soundQueued[static_cast<int>(SoundChannel::Digital)] <= lastChunkSeconds_;
    }
    return platform.ticks - lastFrameTick_ >= static_cast<std::uint32_t>(options_.delayTicks);
}

bool HnmPlayer::step(Platform& platform, Display& display) {
    if (finished()) {
        return false;
    }
    const HnmFrame frame = file_.frame(next_);
    if (!due(platform, frame)) {
        return false;
    }
    show(platform, display, frame, true);
    return true;
}

void HnmPlayer::show(Platform& platform, Display& display, const HnmFrame& frame, bool sendPalette) {
    if (!frame.sound.empty()) {
        if (soundRate_ == 0) {
            // First sound: a silent buffer as long as the chunk plays first.
            if (frame.sound.size() <= kFirstChunkHeader) {
                throw FormatError("hnm: sound chunk too short");
            }
            soundRate_ = Voc::rateFromByte(frame.sound[kRateByte]);
            const auto samples = frame.sound.subspan(kFirstChunkHeader);
            platform.queueSound(SoundChannel::Digital, soundRate_, std::vector<std::uint8_t>(samples.size(), 0x80),
                                true);
            platform.queueSound(SoundChannel::Digital, soundRate_,
                                std::vector<std::uint8_t>(samples.begin(), samples.end()));
            lastChunkSeconds_ = static_cast<double>(samples.size()) / soundRate_;
        } else {
            platform.queueSound(SoundChannel::Digital, soundRate_,
                                std::vector<std::uint8_t>(frame.sound.begin(), frame.sound.end()));
            lastChunkSeconds_ = static_cast<double>(frame.sound.size()) / soundRate_;
        }
    }
    if (!frame.palette.empty()) {
        display.applyPaletteBlocks(frame.palette);
        if (sendPalette) {
            display.flush();
        }
    }
    drawHnmPicture(display.target(), HnmFile::decodePicture(frame.video), options_.spaced, display.originRows());
    // The original checks for the next frame all the time, so a frame
    // comes when it is due. Here the game runs once per app frame; a frame
    // that is late only by that much keeps the original's schedule.
    const auto delay = static_cast<std::uint32_t>(options_.delayTicks);
    if (next_ > 0 && frame.sound.empty() && delay > 0 && platform.ticks - lastFrameTick_ < 2 * delay) {
        lastFrameTick_ += delay;
    } else {
        lastFrameTick_ = platform.ticks;
    }
    ++next_;
}

} // namespace mentat
