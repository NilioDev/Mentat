// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/talker.hpp"

#include "mentat/data/dat_archive.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/data/voc.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mentat {

namespace {

constexpr int kFrameTicks = 16;
constexpr int kFramesPerMood = 4;
constexpr int kRestlessFrames = 8;
constexpr int kRestlessSequence = 4;
constexpr int kVariantSkip = 32; // frames skipped at the start of a variant's sequence
constexpr int kSceneBottom = 152;
constexpr std::uint8_t kMouthsEnd = 0xFF;
constexpr int kNothing = 0x13F; // a left edge that marks "nothing to draw"

// Places in the program's data.
constexpr std::size_t kMouthAreas = 0x27FA; // per character: left, top, right, bottom
constexpr int kMouthAreaCount = 17;
constexpr int kFirstSound = 0xAE; // the effects' resource numbers start after this
constexpr int kCalibrationSound = 0x0B;
constexpr int kMouthsPerCalibration = 32;

bool sameAt(const PortraitPiece& p, const std::vector<PortraitPiece>& list) {
    return std::find(list.begin(), list.end(), p) != list.end();
}

} // namespace

std::string Talker::voiceName(int character, int phrase, bool inside, int variant) {
    const char letter = static_cast<char>('A' + character);
    char name[32];
    std::snprintf(name, sizeof name, "P%c\\P%c%03X%c%s.VOC", letter, letter, phrase & 0xFFF, inside ? 'I' : 'O',
                  variant > 0 ? std::string(1, static_cast<char>('A' + variant)).c_str() : "");
    return name;
}

void Talker::select() { game_.selectSheet(sheet_); }

void Talker::show(int character, int shiftRight, int variant, int mood) {
    if (character != character_) {
        shownPieces_.clear();
    }
    character_ = character;
    variant_ = variant;
    sheet_ = resource::kFirstPortrait + character;
    select();
    anim_ = PortraitAnimation::parse(game_.sheet());
    area_ = anim_->area();
    shiftRight_ = shiftRight;
    if (area_.left < shiftRight) {
        area_.left += shiftRight;
        area_.right = std::min(area_.right + shiftRight, 320);
    }
    mood_ = mood;

    startSequence();
    std::size_t at = next_;
    drawFrame(anim_->readFrame(at), false);
    Display& d = game_.display();
    if (&d.back() != &d.screen()) {
        d.flush();
        d.copyRect(d.back(), d.screen(), ClipRect{0, 0, 320, kSceneBottom});
    }
}

int Talker::pickSequence() {
    if (variant_ != 0) {
        return (variant_ - 1) * 2;
    }
    return game_.random(6) / 2;
}

void Talker::startSequence() {
    framesLeft_ = mood_ * kFramesPerMood;
    active_ = true;
    restless_ = false;
    next_ = anim_->sequenceStart(pickSequence());
    if (variant_ != 0) {
        anim_->skipFrames(next_, kVariantSkip);
    }
    sequenceStart_ = next_;
    if (!timerRunning_) {
        timerRunning_ = true;
        game_.addTimer(kAnimationTimer, kFrameTicks, [this] { tick(); });
    }
}

// The pieces that differ between two frames, as one rectangle on the
// screen; left is kNothing when they are the same.
ClipRect Talker::piecesBounds(const std::vector<PortraitPiece>& a, const std::vector<PortraitPiece>& b) const {
    ClipRect r{kNothing, 199, 0, 0};
    const auto grow = [&](const std::vector<PortraitPiece>& from, const std::vector<PortraitPiece>& other) {
        for (const PortraitPiece& p : from) {
            if (sameAt(p, other)) {
                continue;
            }
            const SpriteView v = game_.sheet().sprite(static_cast<std::size_t>(p.sprite));
            const int x = p.x + area_.left;
            const int y = p.y + area_.top;
            r.left = std::min(r.left, x);
            r.top = std::min(r.top, y);
            r.right = std::max(r.right, x + v.header.width());
            r.bottom = std::max(r.bottom, y + v.header.height);
        }
    };
    grow(a, b);
    grow(b, a);
    return r;
}

void Talker::drawPieces(const std::vector<PortraitPiece>& pieces, const ClipRect& clip, bool onScreen) {
    Display& d = game_.display();
    const Canvas canvas(onScreen ? d.screen() : d.target(), d.originRows());
    for (const PortraitPiece& p : pieces) {
        drawSpriteClipped(canvas, game_.sheet().sprite(static_cast<std::size_t>(p.sprite)), p.x + area_.left,
                          p.y + area_.top, clip);
    }
}

// Redraws the part of the portrait that changes from the frame shown to
// `frame`: the background comes back there and every piece of the new
// frame is drawn over it, clipped to that part.
void Talker::drawFrame(std::vector<PortraitPiece> frame, bool toScreen) {
    ClipRect dirty;
    if (shownPieces_.empty()) {
        dirty = area_;
    } else {
        dirty = piecesBounds(shownPieces_, frame);
        if (speaking_ && character_ != 9 && character_ != 12 && character_ < kMouthAreaCount) {
            // The mouth is the voice's: the animation keeps above it.
            const int mouthTop = game_.resources().program().dataU16(kMouthAreas + 8 * static_cast<std::size_t>(character_) + 2);
            if (mouthTop < dirty.bottom) {
                dirty.bottom = mouthTop;
                if (mouthTop <= dirty.top) {
                    dirty.left = kNothing;
                }
            }
        }
        if (dirty.left == kNothing) {
            return;
        }
    }
    dirty.bottom = std::min(dirty.bottom, kSceneBottom);
    Display& d = game_.display();
    d.copyRect(d.saved(), d.back(), dirty);
    shownPieces_ = std::move(frame);
    drawPieces(shownPieces_, dirty, false);
    if (overlay) {
        overlay(dirty);
    }
    if (toScreen) {
        d.copyRect(d.back(), d.screen(), dirty);
    }
}

void Talker::playFrame() {
    if (anim_->endsAt(next_)) {
        framesLeft_ = 0;
        return;
    }
    --framesLeft_;
    drawFrame(anim_->readFrame(next_), true);
}

void Talker::tick() {
    select();
    if (!active_) {
        return;
    }
    if (!restless_) {
        while (anim_->endsAt(next_)) {
            if (framesLeft_ < 0) {
                restless_ = true;
                break;
            }
            next_ = anim_->sequenceStart(pickSequence());
            if (variant_ != 0) {
                anim_->skipFrames(next_, kVariantSkip);
            }
        }
        if (!restless_) {
            playFrame();
            return;
        }
    }
    if (framesLeft_ > 0) {
        playFrame();
        return;
    }
    // Now and then a short movement from the restless sequence.
    const int sequence = variant_ != 0 ? variant_ - 1 : kRestlessSequence;
    const int r = game_.random(variant_ != 0 ? 0x0F18 : 0x0F38);
    if ((r >> 8) != 0) {
        return;
    }
    next_ = anim_->sequenceStart(sequence);
    anim_->skipFrames(next_, r & 0xFF);
    framesLeft_ = kRestlessFrames;
    playFrame();
}

// --- Speech

double Talker::ticksPerMouthShape() {
    if (mouthTicks_ == 0) {
        mouthTicks_ = 8;
        try {
            const VocSound s = Voc::parse(game_.resources().load(kFirstSound + kCalibrationSound));
            if (s.seconds() > 0) {
                mouthTicks_ = s.seconds() * kTicksPerSecond / kMouthsPerCalibration;
            }
        } catch (const FormatError&) {
        }
    }
    return mouthTicks_;
}

void Talker::restart(int mood) {
    if (character_ < 0) {
        return;
    }
    shownPieces_.clear();
    show(character_, shiftRight_, variant_, mood);
}

void Talker::redraw() {
    if (character_ < 0 || !anim_) {
        return;
    }
    select();
    shownPieces_.clear();
    framesLeft_ = mood_ * kFramesPerMood;
    restless_ = false;
    active_ = true;
    next_ = sequenceStart_;
    std::size_t at = next_;
    drawFrame(anim_->readFrame(at), false);
}

void Talker::halt() {
    endSpeech();
    active_ = false;
    restless_ = false;
    framesLeft_ = 0;
    if (timerRunning_) {
        timerRunning_ = false;
        game_.removeTimer(kAnimationTimer);
    }
    character_ = -1;
    shownPieces_.clear();
}

bool Talker::speak(int phrase, int variant) {
    endSpeech();
    if (character_ < 0) {
        return false;
    }
    DatArchive& archive = game_.resources().archive();
    std::string name = voiceName(character_, phrase, game_.indoors, variant);
    if (archive.directory().find(name) == nullptr) {
        name = voiceName(character_, phrase, !game_.indoors, variant);
        if (archive.directory().find(name) == nullptr) {
            return false;
        }
    }
    const VocSound voice = Voc::parse(archive.read(name));

    // The face starts moving as it speaks.
    if (character_ < 0x10) {
        select();
        restless_ = true;
        const int sequence = variant_ != 0 ? variant_ - 1 : kRestlessSequence;
        const int r = game_.random(variant_ != 0 ? 0x0F18 : 0x0F38);
        next_ = anim_->sequenceStart(sequence);
        anim_->skipFrames(next_, r & 0xFF);
        framesLeft_ = kRestlessFrames;
    }

    Platform& p = game_.platform();
    game_.stopSound();
    for (const VocPart& part : voice.parts) {
        p.queueSound(SoundChannel::Digital, part.rate, part.samples);
    }
    mouths_.clear();
    if (voice.text.size() > 2) {
        mouths_.assign(voice.text.begin() + 2, voice.text.end());
    }
    mouthAt_ = 0;
    lastMouth_ = -1;
    speaking_ = true;
    select();
    game_.addTimer(kSpeechTimer, 0, [this] { speechTick(); });
    nextMouthTick_ = p.ticks + 0.5;
    if (mouthAt_ < mouths_.size() && mouths_[mouthAt_] != kMouthsEnd) {
        showMouth(mouths_[mouthAt_++]);
    }
    return true;
}

bool Talker::playVoice(int character, int phrase, int variant) {
    endSpeech();
    DatArchive& archive = game_.resources().archive();
    std::string name = voiceName(character, phrase, game_.indoors, variant);
    if (archive.directory().find(name) == nullptr) {
        name = voiceName(character, phrase, !game_.indoors, variant);
        if (archive.directory().find(name) == nullptr) {
            return false;
        }
    }
    const VocSound voice = Voc::parse(archive.read(name));
    Platform& p = game_.platform();
    game_.stopSound();
    for (const VocPart& part : voice.parts) {
        p.queueSound(SoundChannel::Digital, part.rate, part.samples);
    }
    return true;
}

void Talker::speechTick() {
    if (!speaking_) {
        return;
    }
    Platform& p = game_.platform();
    if (mouths_.empty()) {
        if (p.soundQueued[static_cast<int>(SoundChannel::Digital)] <= 0) {
            endSpeech();
        }
        return;
    }
    const double step = ticksPerMouthShape();
    int shape = -1;
    while (std::floor(nextMouthTick_ + step) <= static_cast<double>(p.ticks)) {
        if (mouthAt_ >= mouths_.size() || mouths_[mouthAt_] == kMouthsEnd) {
            endSpeech();
            return;
        }
        shape = mouths_[mouthAt_++];
        nextMouthTick_ += step;
    }
    if (shape >= 0) {
        showMouth(shape);
    }
}

void Talker::showMouth(int shape) {
    if (shape == lastMouth_) {
        return;
    }
    lastMouth_ = shape;
    if (character_ < 0 || character_ >= kMouthAreaCount || !anim_) {
        return;
    }
    select();
    const ProgramFile& program = game_.resources().program();
    const std::size_t at = kMouthAreas + 8 * static_cast<std::size_t>(character_);
    const ClipRect rect{program.dataU16(at) + area_.left, program.dataU16(at + 2) + area_.top,
                        program.dataU16(at + 4) + area_.left, program.dataU16(at + 6) + area_.top};
    int index = shape;
    if (variant_ > 0) {
        index += 4 * (variant_ - 1);
    }
    const std::vector<PortraitPiece> pieces = anim_->mouth(index);
    Display& d = game_.display();
    if (pieces.size() >= 2) {
        drawPieces(pieces, rect, false);
        d.copyRect(d.back(), d.screen(), rect);
    } else {
        drawPieces(pieces, rect, true);
    }
}

void Talker::endSpeech() {
    if (!speaking_) {
        return;
    }
    speaking_ = false;
    game_.removeTimer(kSpeechTimer);
    mouths_.clear();
    game_.platform().stopSound(SoundChannel::Digital);
}

void Talker::stop() {
    // The animation first plays on to a whole group of eight frames, as
    // the original does when it leaves a scene.
    for (int guard = 0; active_ && (framesLeft_ & 7) != 0 && guard < 400; ++guard) {
        tick();
    }
    endSpeech();
    active_ = false;
    restless_ = false;
    framesLeft_ = 0;
    if (timerRunning_) {
        timerRunning_ = false;
        game_.removeTimer(kAnimationTimer);
    }
}

} // namespace mentat
