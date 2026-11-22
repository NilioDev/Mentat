// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/videos.hpp"

#include "mentat/data/program_tables.hpp"
#include "mentat/engine/display.hpp"
#include "mentat/engine/hnm_player.hpp"

namespace mentat {

bool playVideo(Platform& platform, DatArchive& archive, const ProgramFile& program, int index) {
    const VideoInfo info = videoInfo(program, index);
    if (info.name.empty()) {
        return true;
    }
    HnmPlayer player(HnmFile::parse(archive.read(info.name)), HnmPlayer::Options{info.delay, info.spaced});
    platform.takeSkip();
    if (info.spaced) {
        platform.screen.fill(0);
    }
    Display display(platform, DisplayTables{});
    display.palette() = platform.palette;
    display.targetScreen();
    player.start(platform, display);
    display.flush();
    while (!player.finished()) {
        platform.waitFrame();
        if (platform.takeSkip()) {
            platform.stopSound(SoundChannel::Digital);
            return false;
        }
        player.step(platform, display);
    }
    // Let the last sound finish.
    while (platform.soundQueued[static_cast<int>(SoundChannel::Digital)] > 0) {
        platform.waitFrame();
        if (platform.takeSkip()) {
            platform.stopSound(SoundChannel::Digital);
            return false;
        }
    }
    return true;
}

} // namespace mentat
