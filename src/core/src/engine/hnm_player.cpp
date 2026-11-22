// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/hnm_player.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/data/voc.hpp"
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

void HnmPlayer::start(Platform& platform, Surface& target, Palette& palette) {
    palette.applyBlocks(file_.headerPalette());
    next_ = 0;
    soundRate_ = 0;
    if (file_.frameCount() > 0) {
        show(platform, target, palette, file_.frame(0));
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

bool HnmPlayer::step(Platform& platform, Surface& target, Palette& palette) {
    if (finished()) {
        return false;
    }
    const HnmFrame frame = file_.frame(next_);
    if (!due(platform, frame)) {
        return false;
    }
    show(platform, target, palette, frame);
    return true;
}

void HnmPlayer::show(Platform& platform, Surface& target, Palette& palette, const HnmFrame& frame) {
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
        palette.applyBlocks(frame.palette);
    }
    drawHnmPicture(target, HnmFile::decodePicture(frame.video), options_.spaced);
    lastFrameTick_ = platform.ticks;
    ++next_;
}

} // namespace mentat
