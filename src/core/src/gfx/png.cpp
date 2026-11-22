// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/png.hpp"

#include "mentat/gfx/palette.hpp"
#include "mentat/gfx/surface.hpp"

#include <array>
#include <fstream>
#include <stdexcept>

namespace mentat {

namespace {

std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc = 0) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) {
        out.push_back(static_cast<std::uint8_t>(v >> s));
    }
}

void chunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& body) {
    put32(out, static_cast<std::uint32_t>(body.size()));
    std::vector<std::uint8_t> crcData(type, type + 4);
    crcData.insert(crcData.end(), body.begin(), body.end());
    out.insert(out.end(), crcData.begin(), crcData.end());
    put32(out, crc32(crcData.data(), crcData.size()));
}

} // namespace

std::vector<std::uint8_t> encodePng(const Surface& s, const Palette& pal) {
    std::vector<std::uint8_t> raw;
    const auto w = static_cast<std::size_t>(s.width());
    const auto h = static_cast<std::size_t>(s.height());
    raw.reserve((w * 3 + 1) * h);
    for (std::size_t y = 0; y < h; ++y) {
        raw.push_back(0);
        for (std::size_t x = 0; x < w; ++x) {
            const Colour& c = pal[s.data()[y * w + x]];
            raw.push_back(Palette::to8Bit(c.r));
            raw.push_back(Palette::to8Bit(c.g));
            raw.push_back(Palette::to8Bit(c.b));
        }
    }
    // zlib stream of stored blocks.
    std::vector<std::uint8_t> z{0x78, 0x01};
    std::size_t pos = 0;
    do {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - pos);
        const bool last = pos + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n));
        z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n));
        z.push_back(static_cast<std::uint8_t>(~n >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    } while (pos < raw.size());
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    put32(z, (b << 16) | a);

    std::vector<std::uint8_t> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> ihdr;
    put32(ihdr, static_cast<std::uint32_t>(w));
    put32(ihdr, static_cast<std::uint32_t>(h));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

void writePng(const std::filesystem::path& path, const Surface& surface, const Palette& palette) {
    const auto bytes = encodePng(surface, palette);
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot write " + path.string());
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

} // namespace mentat
