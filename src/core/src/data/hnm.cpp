// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/hnm.hpp"

#include "mentat/data/hsq.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/gfx/surface.hpp"

namespace mentat {

namespace {

constexpr std::uint16_t kTagSound = 0x6473;   // "sd"
constexpr std::uint16_t kTagPalette = 0x6C70; // "pl"
constexpr std::uint16_t kWholePacked = 0x0400; // first word: the whole frame is HSQ packed
constexpr std::uint16_t kRestPacked = 0x0200;  // first word: the rest of the picture is HSQ packed

} // namespace

HnmFile HnmFile::parse(std::vector<std::uint8_t> bytes) {
    HnmFile f;
    f.bytes_ = std::move(bytes);
    const std::span<const std::uint8_t> all(f.bytes_);
    Reader r(all);
    const std::size_t headerSize = r.u16();
    if (headerSize < 4 || headerSize > all.size()) {
        throw FormatError("HNM header larger than the file");
    }
    // The header palette ends with an 0xFFFF word; check it parses.
    Reader pal(all.subspan(2, headerSize - 2));
    Palette scratch;
    scratch.applyBlocks(pal);
    f.headerPalette_ = all.subspan(2, pal.pos());

    std::size_t pos = headerSize;
    while (pos + 2 <= all.size()) {
        const std::size_t size = static_cast<std::size_t>(all[pos]) | (static_cast<std::size_t>(all[pos + 1]) << 8);
        if (size < 2 || pos + size > all.size()) {
            throw FormatError("HNM frame runs past the end of the file");
        }
        f.frames_.push_back({pos + 2, pos + size});
        pos += size;
    }
    return f;
}

HnmFrame HnmFile::frame(std::size_t index) const {
    if (index >= frames_.size()) {
        throw FormatError("HNM frame out of range");
    }
    const std::span<const std::uint8_t> all(bytes_);
    const auto body = all.subspan(frames_[index].begin, frames_[index].end - frames_[index].begin);
    HnmFrame out;
    std::size_t pos = 0;
    while (pos + 4 <= body.size()) {
        const std::uint16_t tag = static_cast<std::uint16_t>(body[pos] | (body[pos + 1] << 8));
        if (tag != kTagSound && tag != kTagPalette) {
            break;
        }
        const std::size_t size = static_cast<std::size_t>(body[pos + 2]) | (static_cast<std::size_t>(body[pos + 3]) << 8);
        if (size < 4 || pos + size > body.size()) {
            throw FormatError("HNM chunk runs past the end of its frame");
        }
        (tag == kTagSound ? out.sound : out.palette) = body.subspan(pos + 4, size - 4);
        pos += size;
    }
    out.video = body.subspan(pos);
    return out;
}

HnmPicture HnmFile::decodePicture(std::span<const std::uint8_t> video) {
    HnmPicture pic;
    if (video.size() <= 4) {
        return pic; // no picture, or a frame that only repeats the last one
    }
    Reader r(video);
    const std::uint16_t first = r.u16();
    const std::uint16_t second = r.u16();
    if (first & kWholePacked) {
        pic.kind = HnmPicture::Kind::FullFrame;
        pic.width = first & SpriteHeader::kWidthMask;
        pic.height = second & 0xFF;
        pic.bytes = unpackHsq(video.subspan(4));
        if (pic.bytes.size() < static_cast<std::size_t>(pic.width) * static_cast<std::size_t>(pic.height)) {
            throw FormatError("HNM frame smaller than its size says");
        }
        return pic;
    }
    if (first == 0) {
        return pic;
    }
    std::vector<std::uint8_t> rest;
    if (first & kRestPacked) {
        rest = unpackHsq(video.subspan(4));
    } else {
        rest.assign(video.begin() + 4, video.end());
    }
    Reader rr(rest);
    pic.kind = HnmPicture::Kind::Sprite;
    pic.x = rr.u16();
    pic.y = rr.u16();
    const std::uint16_t widthAndFlags = static_cast<std::uint16_t>(first & ~(kWholePacked | kRestPacked));
    pic.width = widthAndFlags & SpriteHeader::kWidthMask;
    pic.height = second & 0xFF;
    pic.bytes.reserve(rest.size());
    pic.bytes.push_back(static_cast<std::uint8_t>(widthAndFlags));
    pic.bytes.push_back(static_cast<std::uint8_t>(widthAndFlags >> 8));
    pic.bytes.push_back(static_cast<std::uint8_t>(second));
    pic.bytes.push_back(static_cast<std::uint8_t>(second >> 8));
    pic.bytes.insert(pic.bytes.end(), rest.begin() + 4, rest.end());
    return pic;
}

void drawHnmPicture(Surface& to, const HnmPicture& pic, bool spaced) {
    switch (pic.kind) {
    case HnmPicture::Kind::None:
        return;
    case HnmPicture::Kind::FullFrame:
        for (int y = 0; y < pic.height; ++y) {
            for (int x = 0; x < pic.width; ++x) {
                to.set(x, y, pic.bytes[static_cast<std::size_t>(y) * static_cast<std::size_t>(pic.width) + static_cast<std::size_t>(x)]);
            }
        }
        return;
    case HnmPicture::Kind::Sprite: {
        const SpriteView view = readSprite(pic.bytes);
        if (!spaced) {
            drawSprite(to, view, pic.x, pic.y);
            return;
        }
        const SpritePixels px = decodeSprite(view);
        for (int y = 0; y < px.height; ++y) {
            for (int x = 0; x < px.drawnWidth; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(px.width) + static_cast<std::size_t>(x);
                to.set(pic.x + 2 * x, pic.y + 2 * y, px.pixels[i]);
            }
        }
        return;
    }
    }
}

} // namespace mentat
