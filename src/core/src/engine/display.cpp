// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/display.hpp"

#include "mentat/data/reader.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace mentat {

namespace {

constexpr int kSceneRows = 152;
constexpr std::uint32_t kStepTicks = 3;

// Component `i` (0..767) of a palette, as the original keeps them: red,
// green and blue of colour 0, then of colour 1, and so on.
std::uint8_t& component(Palette& p, int i) {
    Colour& c = p[static_cast<std::size_t>(i / 3)];
    switch (i % 3) {
    case 0:
        return c.r;
    case 1:
        return c.g;
    default:
        return c.b;
    }
}

} // namespace

DisplayTables DisplayTables::fromDriver(std::span<const std::uint8_t> driver) {
    constexpr std::size_t kDissolveOrder = 0x2ED7; // in the file; the driver runs from offset 0x100
    Reader r(driver);
    r.seek(kDissolveOrder);
    DisplayTables t;
    for (int& place : t.dissolveOrder) {
        place = r.u16();
        if (place % Surface::kScreenWidth > 3 || place / Surface::kScreenWidth > 3) {
            throw FormatError("video driver: unexpected dissolve order");
        }
    }
    if (r.u16() != 0xFFFF) {
        throw FormatError("video driver: unexpected dissolve order");
    }
    return t;
}

Display::Display(Platform& platform, const DisplayTables& tables)
    : platform_(platform), tables_(tables), back_(&backStore_), screen_(&platform.screen), target_(&backStore_) {}

void Display::showBack() {
    if (back_ != screen_) {
        *screen_ = *back_;
    }
}

void Display::markChanged(const std::bitset<256>& colours) {
    dirty_ |= colours;
    if (colours.any()) {
        anyDirty_ = true;
    }
}

void Display::applyPaletteBlocks(std::span<const std::uint8_t> blocks) {
    std::bitset<256> touched;
    shadow_.applyBlocks(blocks, &touched);
    markChanged(touched);
}

void Display::flush() {
    if (!anyDirty_) {
        return;
    }
    anyDirty_ = false;
    for (std::size_t i = 0; i < 256; ++i) {
        if (dirty_.test(i)) {
            platform_.palette[i] = shadow_[i];
        }
    }
    dirty_.reset();
}

void Display::sendColours(int first, int count) {
    for (int i = first; i < first + count && i < 256; ++i) {
        platform_.palette[static_cast<std::size_t>(i)] = shadow_[static_cast<std::size_t>(i)];
    }
}

void Display::fadeStep(int first, int count, int divisor) {
    if (divisor == 0) {
        divisor = 1;
    }
    for (int i = first * 3; i < (first + count) * 3 && i < 768; ++i) {
        std::uint8_t& v = component(shadow_, i);
        const int distance = static_cast<std::int8_t>(static_cast<std::uint8_t>(component(spare_, i) - v));
        v = static_cast<std::uint8_t>(v + distance / divisor);
    }
    sendColours(first, count);
}

// Steps are timed from when the last one was due, not from when the app
// got round to it, so they keep the original's pace.
std::uint32_t Display::waitUntil(std::uint32_t due) {
    while (static_cast<std::int32_t>(due - platform_.ticks) > 0) {
        platform_.waitFrame();
    }
    return due;
}

std::uint32_t Display::waitTicks(std::uint32_t since, std::uint32_t ticks) {
    while (platform_.ticks - since < ticks) {
        platform_.waitFrame();
    }
    return platform_.ticks;
}

// Takes `step` off every component, `chunk` colours at a time, `rounds`
// times. Each chunk is sent to the screen and followed by a short wait.
// Colour 255 is never reached when the chunks are 85 colours long.
void Display::fadeDown(int chunk, int step, int rounds) {
    std::uint32_t due = platform_.ticks;
    for (int round = 0; round < rounds; ++round) {
        for (int first = 0; first < 0xFF; first += chunk) {
            for (int i = first * 3; i < (first + chunk) * 3 && i < 768; ++i) {
                std::uint8_t& v = component(shadow_, i);
                v = v > step ? static_cast<std::uint8_t>(v - step) : 0;
            }
            sendColours(first, chunk);
            due = waitUntil(due + kStepTicks);
        }
    }
}

// Brings colours 0..254 up towards the spare palette over `rounds` rounds,
// `chunk` colours at a time. A component starts to rise only when the
// rounds left are just enough to cover its distance in steps of `step`
// (the odd remainder goes first), so all of them arrive together.
void Display::fadeUp(int chunk, int step, int rounds) {
    std::uint32_t due = platform_.ticks;
    for (int left = rounds; left > 0; --left) {
        for (int first = 0; first * 3 < 0x2FD; first += chunk) {
            for (int i = first * 3; i < (first + chunk) * 3 && i < 768; ++i) {
                std::uint8_t& v = component(shadow_, i);
                const std::uint8_t distance = static_cast<std::uint8_t>(component(spare_, i) - v);
                if (distance == 0) {
                    continue;
                }
                int add = distance % step;
                int steps = distance / step + 1;
                if (add == 0) {
                    add = step;
                    --steps;
                }
                if (steps >= left) {
                    v = static_cast<std::uint8_t>(v + add);
                }
            }
            sendColours(first, chunk);
            due = waitUntil(due + kStepTicks);
        }
    }
}

// Blacks out the scene area through a 4 × 4 pattern, one pattern position
// at a time, then shows the back buffer through the same pattern.
void Display::dissolve() {
    const int origin = originRows_ * Surface::kScreenWidth;
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            flush();
        }
        std::uint32_t due = platform_.ticks;
        for (const int offset : tables_.dissolveOrder) {
            for (int block = 0; block < kSceneRows / 4; ++block) {
                for (int column = 0; column < 80; ++column) {
                    const std::size_t at = static_cast<std::size_t>(origin + offset + block * 0x500 + column * 4);
                    if (at < screen_->size()) {
                        screen_->data()[at] = pass == 0 ? 0 : back_->data()[at];
                    }
                }
            }
            due = waitUntil(due + kStepTicks);
        }
    }
}

void Display::transition(int type) {
    switch (type & 0xFE) {
    case kFadeThroughBlack:
        // Colours 1..254 of the new palette wait in the spare one while
        // the old picture fades out.
        for (std::size_t i = 1; i < 255; ++i) {
            std::swap(shadow_[i], spare_[i]);
        }
        fadeDown(0x55, 3, 0x16);
        showBack();
        fadeUp(0x55, 3, 0x16);
        break;
    case kCut: {
        flush();
        const std::size_t origin = static_cast<std::size_t>(originRows_) * Surface::kScreenWidth;
        const std::size_t count = std::min<std::size_t>(kSceneRows * Surface::kScreenWidth, screen_->size() - origin);
        std::copy_n(back_->data() + origin, count, screen_->data() + origin);
        break;
    }
    case kFadeInFromBlack:
        spare_ = shadow_;
        dirty_.reset();
        for (std::size_t i = 0; i < 255; ++i) {
            shadow_[i] = Colour{};
        }
        sendColours(0, 255);
        showBack();
        fadeUp(0x20, 1, 0x40);
        break;
    case kFadeToBlack:
        spare_ = shadow_;
        fadeDown(0x40, 2, 0x20);
        showBack();
        shadow_ = spare_;
        flush();
        break;
    case kDissolve:
        dissolve();
        break;
    default:
        throw FormatError("display: unknown transition " + std::to_string(type));
    }
    showBack();
    flush();
}

} // namespace mentat
