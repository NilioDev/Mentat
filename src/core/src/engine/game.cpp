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

namespace {

// The mixer panel's controls in the program's data: a volume byte each,
// with its balance 0x18 bytes further.
constexpr std::size_t kVoicesLevel = 0x288E;
constexpr std::size_t kMusicLevel = 0x2896;
constexpr std::size_t kQuietMusicLevel = 0x289E;
constexpr std::size_t kBalanceAfter = 0x18;
// How fast the music goes quiet for a voice, and back.
constexpr int kQuietTicks = 0x64;
constexpr int kLoudTicks = 0x190;
constexpr int kFremenLanguage = 6;

} // namespace

Game::Game(Platform& platform, DatArchive& archive, const ProgramFile& program, const DisplayTables& displayTables,
           MusicSystem* music)
    : platform_(platform), resources_(archive, program), display_(platform, displayTables),
      sceneTables_(SceneTables::fromProgram(program)), music_(music), timersRan_(platform.ticks),
      talker_(std::make_unique<Talker>(*this)), companion_(std::make_unique<Talker>(*this)), places_(std::make_unique<Places>(*this)) {
    // The mixer panel's settings start as the program keeps them.
    levels.voices = program.dataU8(kVoicesLevel);
    levels.voicesBalance = program.dataU8(kVoicesLevel + kBalanceAfter);
    levels.music = program.dataU8(kMusicLevel);
    levels.musicBalance = program.dataU8(kMusicLevel + kBalanceAfter);
    levels.quietMusic = program.dataU8(kQuietMusicLevel);
    levels.quietBalance = program.dataU8(kQuietMusicLevel + kBalanceAfter);
    applyVoiceLevels();
}

Game::~Game() = default;

int Game::fontFile() const { return language == kFremenLanguage ? resource::kFremenFont : resource::kFont; }

void Game::applyVoiceLevels() {
    platform_.digitalLevels = MusicDriver::mixerLevels(levels.voices, levels.voicesBalance);
}

void Game::applyMusicLevels() {
    if (music_ != nullptr) {
        music_->setVolume(std::max<std::uint8_t>(levels.music, 4), levels.musicBalance);
    }
}

void Game::quietenForVoice() {
    quietForVoice_ = true;
    fadeMusic(kQuietTicks, levels.quietMusic, levels.quietBalance);
}

void Game::checkVoiceEnded() {
    if (quietForVoice_ && platform_.soundQueued[static_cast<int>(SoundChannel::Digital)] <= 0) {
        quietForVoice_ = false;
        fadeMusic(kLoudTicks, levels.music, levels.musicBalance);
    }
}

namespace {

// The number a generator gives: the middle two bytes of the product, its
// low word counted after the 1 was added.
int middle(std::uint32_t product) {
    const std::uint32_t low = (product + 1) & 0xFFFF;
    return static_cast<int>((low >> 8) | (((product >> 16) & 0xFF) << 8));
}

} // namespace

// The original's generator: the seed goes through seed × 0xE56D + 1, and
// the middle two bytes of the product are the number.
int Game::random(int mask) {
    const std::uint32_t product = static_cast<std::uint32_t>(randomSeed_) * 0xE56Du;
    randomSeed_ = static_cast<std::uint16_t>(product + 1);
    return middle(product) & mask;
}

// The second generator: seed × 0xCBD1 + 1, again the middle two bytes.
int Game::random2() {
    const std::uint32_t product = static_cast<std::uint32_t>(randomSeed2_) * 0xCBD1u;
    randomSeed2_ = static_cast<std::uint16_t>(product + 1);
    return middle(product);
}

// The third generator: as the second, from its own seed, the number
// masked to the bits that `most` needs and drawn again while above it.
int Game::random3(int most) {
    if (most <= 0) {
        return 0;
    }
    int mask = 0;
    while (mask < most) {
        mask = mask * 2 + 1;
    }
    for (;;) {
        const std::uint32_t product = static_cast<std::uint32_t>(randomSeed3_) * 0xCBD1u;
        randomSeed3_ = static_cast<std::uint16_t>(product + 1);
        const int n = middle(product) & mask;
        if (n <= most) {
            return n;
        }
    }
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
    if (music_ == nullptr || musicOff || n == track_) {
        return;
    }
    track_ = n;
    const int index = resource::kFirstMusic + n;
    std::vector<std::uint8_t> bytes;
    if (music_->card() == MusicCard::AdLibGold) {
        // The AdLib Gold plays its own arrangement: the same name with .AGD.
        std::string name = resources_.name(index);
        name = name.substr(0, name.rfind('.')) + ".AGD";
        bytes = resources_.archive().load(name);
    } else {
        bytes = resources_.load(index);
    }
    auto song = std::make_shared<const Song>(Song::parse(std::move(bytes)));
    music_->start(std::move(song), playTracksOnce ? 1 : 0);
}

void Game::stopMusic() {
    track_ = 0;
    if (music_ != nullptr) {
        music_->stop();
    }
}

void Game::switchMusic(MusicSystem* next) {
    const int piece = track_;
    const bool playing = musicPlaying();
    if (music_ != nullptr) {
        music_->stop();
    }
    music_ = next;
    platform_.music = next;
    track_ = 0;
    if (music_ == nullptr) {
        return;
    }
    if (quietForVoice_) {
        music_->setVolume(std::max<std::uint8_t>(levels.quietMusic, 4), levels.quietBalance);
    } else {
        applyMusicLevels();
    }
    if (playing && piece != 0) {
        playTrack(piece);
    }
}

void Game::fadeMusic(int ticks, std::uint8_t volume, std::uint8_t balance) {
    if (music_ != nullptr) {
        // The player takes no volume below 4.
        music_->fade(ticks, std::max<std::uint8_t>(volume, 4), balance);
    }
}

bool Game::musicPlaying() const { return music_ != nullptr && music_->status().playing(); }

int Game::musicMeasure() const { return music_ != nullptr ? music_->status().measure : 0; }

int Game::musicPosition() const { return music_ != nullptr ? music_->status().position() : 0; }

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
namespace {
constexpr std::uint32_t kMaxRunsPerCheck = 64;
} // namespace

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
        // The original checks many times a tick, so a job falls due at
        // most once a tick; our checks come a few ticks apart, so a job
        // with a short interval runs once for each interval that passed.
        std::uint32_t runs = 1;
        if (it->interval != 0) {
            runs = std::min<std::uint32_t>(count / it->interval, kMaxRunsPerCheck);
            it->count = count % it->interval;
        } else {
            it->count = count;
        }
        const TimerFn fn = it->fn;
        for (std::uint32_t r = 0; r < runs; ++r) {
            // A run may remove its own timer.
            if (r > 0 && std::none_of(timers_.begin(), timers_.end(), [id](const Timer& t) { return t.id == id; })) {
                break;
            }
            fn();
        }
    }
}

// --- Waiting

bool Game::pollInput() {
    if (platform_.takeSkip()) {
        return true;
    }
    keepLooping();
    runTimers();
    checkVoiceEnded();
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
        return !repeating() && platform_.soundQueued[static_cast<int>(SoundChannel::Digital)] > 0;
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
    VocSound sound = Voc::parse(resources_.load(kFirstSound + n));
    stopSound();
    soundParts_ = std::move(sound.parts);
    soundPart_ = 0;
    playsLeft_ = soundParts_.empty() ? 0 : soundParts_[0].plays;
    keepLooping();
}

void Game::endSoundRepeat() {
    if (!repeating() || soundParts_[soundPart_].plays == 1) {
        return;
    }
    // What is queued plays out; then the next part follows.
    ++soundPart_;
    if (soundPart_ < soundParts_.size()) {
        playsLeft_ = soundParts_[soundPart_].plays;
    }
    keepLooping();
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

bool Game::waitSound() {
    while (platform_.soundQueued[static_cast<int>(SoundChannel::Digital)] > 0) {
        if (pollInput()) {
            return false;
        }
        platform_.waitFrame();
    }
    return true;
}

void Game::stopSound() {
    soundParts_.clear();
    soundPart_ = 0;
    playsLeft_ = 0;
    platform_.stopSound(SoundChannel::Digital);
}

// Queues the effect's parts as they come: a part played once at once, a
// part that repeats a play at a time, kept a little ahead.
void Game::keepLooping() {
    constexpr double kAhead = 0.05; // seconds kept queued of a repeating part
    double& queued = platform_.soundQueued[static_cast<int>(SoundChannel::Digital)];
    while (soundPart_ < soundParts_.size()) {
        const VocPart& part = soundParts_[soundPart_];
        const bool empty = part.rate <= 0 || part.samples.empty();
        if (!empty) {
            if (part.plays != 1 && queued >= kAhead) {
                return;
            }
            platform_.queueSound(SoundChannel::Digital, part.rate, part.samples);
            if (part.plays == 0 || --playsLeft_ > 0) {
                continue; // until stopped, or plays left
            }
        }
        ++soundPart_;
        if (soundPart_ < soundParts_.size()) {
            playsLeft_ = soundParts_[soundPart_].plays;
        }
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
