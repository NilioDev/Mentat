// SPDX-License-Identifier: GPL-3.0-or-later
//
// Plays an HNM video frame by frame with the original's timing. See
// docs/rules/timing.md.
#pragma once

#include "mentat/data/hnm.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/gfx/palette.hpp"

#include <cstdint>

namespace mentat {

class Display;
class Surface;

class HnmPlayer {
public:
    struct Options {
        int delayTicks = 0;  // timer ticks between frames without sound
        bool spaced = false; // draw each pixel in every second column and row
    };

    HnmPlayer(HnmFile file, Options options);

    // Opens the video the way the original does: the header palette goes
    // into the display's palette (not yet sent to the screen) and the first
    // frame is drawn on the display's target, its palette part also going
    // into the display's palette unsent.
    void start(Platform& platform, Display& display);

    // Draws the next frame on the display's target when it is due; its
    // palette part is sent to the screen first. True when it drew one.
    bool step(Platform& platform, Display& display);

    bool finished() const { return next_ >= file_.frameCount(); }
    std::size_t framesShown() const { return next_; }
    const HnmFile& file() const { return file_; }

private:
    bool due(const Platform& platform, const HnmFrame& frame) const;
    void show(Platform& platform, Display& display, const HnmFrame& frame, bool sendPalette);

    HnmFile file_;
    Options options_;
    std::size_t next_ = 0;
    std::uint32_t lastFrameTick_ = 0;
    int soundRate_ = 0;
    double lastChunkSeconds_ = 0;
};

} // namespace mentat
