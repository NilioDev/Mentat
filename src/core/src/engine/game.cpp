// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/game.hpp"

#include "mentat/audio/music.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/data/voc.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/talker.hpp"

#include <algorithm>

namespace mentat {

Game::Game(Platform& platform, DatArchive& archive, const ProgramFile& program, const DisplayTables& displayTables,
           MusicSystem* music)
    : platform_(platform), resources_(archive, program), display_(platform, displayTables),
      sceneTables_(SceneTables::fromProgram(program)), music_(music), timersRan_(platform.ticks),
      talker_(std::make_unique<Talker>(*this)), places_(std::make_unique<Places>(*this)) {}

Game::~Game() = default;

// The original's generator: the seed goes through seed × 0xE56D + 1, and
// the middle two bytes of the product are the number.
int Game::random(int mask) {
    const std::uint32_t product = static_cast<std::uint32_t>(randomSeed_) * 0xE56Du;
    randomSeed_ = static_cast<std::uint16_t>(product + 1);
    return static_cast<int>((product >> 8) & 0xFFFF) & mask;
}

// The second generator: seed × 0xCBD1 + 1, again the middle two bytes.
int Game::random2() {
    const std::uint32_t product = static_cast<std::uint32_t>(randomSeed2_) * 0xCBD1u;
    randomSeed2_ = static_cast<std::uint16_t>(product + 1);
    return static_cast<int>((product >> 8) & 0xFFFF);
}

// --- Sprite sheets

void Game::selectSheet(int resource) {
    if (resource < 0 || resource == sheet_) {
        return;
    }
    sheet_ = resource;
    const SpriteSheet& s = resources_.sheet(resource);
    if (s.hasPalette() && paletteSheet_ != resource) {
        paletteSheet_ = resource;
        std::bitset<256> touched;
        s.applyPalette(display_.palette(), &touched);
        display_.markChanged(touched);
    }
}

void Game::selectSheetKeepPalette(int resource) {
    paletteSheet_ = resource;
    selectSheet(resource);
}

const SpriteSheet& Game::sheet() {
    if (sheet_ < 0) {
        throw FormatError("game: no picture file chosen");
    }
    return resources_.sheet(sheet_);
}

void Game::drawSprite(std::uint16_t word, int x, int y) {
    drawSpriteWord(canvas(), sheet(), word, x, y, sceneTables_);
}

void Game::drawSpriteList(std::span<const std::uint16_t> list) {
    for (std::size_t i = 0; i < list.size() && list[i] != 0xFFFF; i += 3) {
        if (i + 2 >= list.size()) {
            throw FormatError("game: sprite list cut short");
        }
        drawSprite(list[i], static_cast<std::int16_t>(list[i + 1]), static_cast<std::int16_t>(list[i + 2]));
    }
}

// --- Music

void Game::playTrack(int n) {
    if (music_ == nullptr || n == track_) {
        return;
    }
    track_ = n;
    const int index = resource::kFirstMusic + n;
    std::vector<std::uint8_t> bytes;
    if (music_->driver().card() == MusicCard::AdLibGold) {
        // The AdLib Gold plays its own arrangement: the same name with .AGD.
        std::string name = resources_.name(index);
        name = name.substr(0, name.rfind('.')) + ".AGD";
        bytes = resources_.archive().load(name);
    } else {
        bytes = resources_.load(index);
    }
    auto song = std::make_shared<const Song>(Song::parse(std::move(bytes)));
    music_->driver().start(music_->opl(), std::move(song), playTracksOnce ? 1 : 0);
}

void Game::stopMusic() {
    track_ = 0;
    if (music_ != nullptr) {
        music_->driver().stop(music_->opl());
    }
}

void Game::fadeMusic(int ticks, std::uint8_t volume, std::uint8_t balance) {
    if (music_ != nullptr) {
        // The player takes no volume below 4.
        music_->driver().fade(ticks, std::max<std::uint8_t>(volume, 4), balance);
    }
}

bool Game::musicPlaying() const { return music_ != nullptr && music_->driver().status().playing(); }

int Game::musicMeasure() const { return music_ != nullptr ? music_->driver().status().measure : 0; }

int Game::musicPosition() const { return music_ != nullptr ? music_->driver().status().position() : 0; }

// --- Timers

void Game::addTimer(int id, int intervalTicks, TimerFn fn) {
    timers_.push_back(Timer{id, static_cast<std::uint32_t>(intervalTicks), 0, std::move(fn)});
}

void Game::removeTimer(int id) {
    timers_.erase(std::remove_if(timers_.begin(), timers_.end(), [id](const Timer& t) { return t.id == id; }),
                  timers_.end());
}

// Each timer adds up the ticks since the timers last ran; when it reaches
// its interval it keeps the remainder and runs.
void Game::runTimers() {
    const std::uint32_t now = platform_.ticks;
    const std::uint32_t elapsed = now - timersRan_;
    timersRan_ = now;
    std::vector<int> ids;
    for (const Timer& t : timers_) {
        ids.push_back(t.id);
    }
    // A timer may add or remove timers while it runs.
    for (const int id : ids) {
        const auto it = std::find_if(timers_.begin(), timers_.end(), [id](const Timer& t) { return t.id == id; });
        if (it == timers_.end()) {
            continue;
        }
        const std::uint32_t count = it->count + elapsed;
        if (count < it->interval) {
            it->count = count;
            continue;
        }
        it->count = it->interval != 0 ? count % it->interval : count;
        const TimerFn fn = it->fn;
        fn();
    }
}

// --- Waiting

bool Game::pollInput() {
    if (platform_.takeSkip()) {
        return true;
    }
    keepLooping();
    runTimers();
    return false;
}

bool Game::waitTicks(int ticks) {
    const std::uint32_t start = platform_.ticks;
    for (;;) {
        if (pollInput()) {
            return false;
        }
        if (platform_.ticks - start >= static_cast<std::uint32_t>(ticks)) {
            return true;
        }
        platform_.waitFrame();
    }
}

bool Game::waitMusic(int position) {
    while (musicPlaying()) {
        if (position <= musicPosition()) {
            return true;
        }
        if (pollInput()) {
            return false;
        }
        platform_.waitFrame();
    }
    return true;
}

bool Game::pause(int ticks) {
    if (musicPlaying()) {
        return true;
    }
    const auto voice = [this] {
        return looping_.samples.empty() && platform_.soundQueued[static_cast<int>(SoundChannel::Digital)] > 0;
    };
    if (!voice()) {
        return waitTicks(ticks);
    }
    while (voice()) {
        if (pollInput()) {
            return false;
        }
        platform_.waitFrame();
    }
    return true;
}

// --- Sound effects

void Game::playSound(int n) {
    constexpr int kFirstSound = 0xAE;
    const VocSound sound = Voc::parse(resources_.load(kFirstSound + n));
    stopSound();
    for (const VocPart& part : sound.parts) {
        platform_.queueSound(SoundChannel::Digital, part.rate, part.samples);
        if (part.plays == 0) {
            // Played until stopped: what follows it is never reached.
            looping_ = part;
            break;
        }
    }
}

bool Game::playVoice(const std::string& name) {
    DatArchive& archive = resources_.archive();
    if (archive.directory().find(name) == nullptr) {
        return false;
    }
    const VocSound sound = Voc::parse(archive.read(name));
    stopSound();
    for (const VocPart& part : sound.parts) {
        platform_.queueSound(SoundChannel::Digital, part.rate, part.samples);
    }
    return true;
}

void Game::stopSound() {
    looping_ = VocPart{};
    platform_.stopSound(SoundChannel::Digital);
}

void Game::keepLooping() {
    constexpr double kAhead = 0.25; // seconds kept queued
    if (looping_.samples.empty() || looping_.rate <= 0) {
        return;
    }
    double& queued = platform_.soundQueued[static_cast<int>(SoundChannel::Digital)];
    while (queued < kAhead) {
        platform_.queueSound(SoundChannel::Digital, looping_.rate, looping_.samples);
    }
}

// --- Videos

void Game::openVideo(int index) {
    closeVideo();
    const VideoInfo info = videoInfo(resources_.program(), index);
    if (info.name.empty()) {
        throw FormatError("game: video slot " + std::to_string(index) + " is empty");
    }
    video_ = std::make_unique<HnmPlayer>(HnmFile::parse(resources_.archive().read(info.name)),
                                         HnmPlayer::Options{info.delay, info.spaced});
    forgetPalette();
    video_->start(platform_, display_);
}

bool Game::videoStep() { return video_ != nullptr && video_->step(platform_, display_); }

int Game::videoFrame() const { return video_ != nullptr ? static_cast<int>(video_->framesShown()) : 0; }

bool Game::videoDone() const { return video_ == nullptr || video_->finished(); }

void Game::closeVideo() { video_.reset(); }

} // namespace mentat
