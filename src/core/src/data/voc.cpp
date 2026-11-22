// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/voc.hpp"

#include "mentat/data/reader.hpp"

#include <algorithm>
#include <cstring>

namespace mentat {

namespace {

constexpr char kSignature[] = "Creative Voice File\x1A";
constexpr std::size_t kSignatureSize = sizeof(kSignature) - 1;

} // namespace

double VocSound::seconds() const {
    double total = 0;
    for (const auto& part : parts) {
        if (part.rate > 0) {
            total += static_cast<double>(part.samples.size()) / part.rate;
        }
    }
    return total;
}

std::vector<std::uint8_t> Voc::unpack4Bit(std::span<const std::uint8_t> data) {
    std::vector<std::uint8_t> out;
    if (data.empty()) {
        return out;
    }
    out.reserve(data.size() * 2);
    int sample = data[0];
    int level = 0; // 0..3: how far each step moves
    out.push_back(static_cast<std::uint8_t>(sample));
    for (std::size_t i = 1; i < data.size(); ++i) {
        for (const int nibble : {data[i] >> 4, data[i] & 0x0F}) {
            const int size = nibble & 7;
            int step = size << level;
            if (level > 0) {
                step += 1 << (level - 1);
            }
            sample = std::clamp(sample + ((nibble & 8) != 0 ? -step : step), 0, 255);
            out.push_back(static_cast<std::uint8_t>(sample));
            if (size >= 5 && level < 3) {
                ++level;
            } else if (size == 0 && level > 0) {
                --level;
            }
        }
    }
    return out;
}

VocSound Voc::parse(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    const auto signature = r.bytes(kSignatureSize);
    if (std::memcmp(signature.data(), kSignature, kSignatureSize) != 0) {
        throw FormatError("voc: not a Creative Voice File");
    }
    const std::uint16_t dataStart = r.u16();
    r.seek(dataStart);

    VocSound sound;
    int lastRate = 0;
    int repeat = -1; // part index where a repeat starts
    int repeatPlays = 1;
    while (r.remaining() > 0) {
        const std::uint8_t type = r.u8();
        if (type == 0) {
            break;
        }
        const std::uint32_t low = r.u16();
        const std::uint32_t length = low | (static_cast<std::uint32_t>(r.u8()) << 16);
        Reader block(r.bytes(length));
        switch (type) {
        case 1: { // sound with a new rate
            const std::uint8_t rateByte = block.u8();
            const std::uint8_t codec = block.u8();
            const auto data = block.bytes(block.remaining());
            VocPart part;
            part.rate = lastRate = rateFromByte(rateByte);
            if (codec == 0) {
                part.samples.assign(data.begin(), data.end());
            } else if (codec == 1) {
                part.samples = unpack4Bit(data);
            } else {
                throw FormatError("voc: unknown packing");
            }
            sound.parts.push_back(std::move(part));
            break;
        }
        case 2: { // more sound at the last rate
            if (lastRate == 0) {
                throw FormatError("voc: continuation before sound");
            }
            const auto data = block.bytes(block.remaining());
            VocPart part;
            part.rate = lastRate;
            part.samples.assign(data.begin(), data.end());
            sound.parts.push_back(std::move(part));
            break;
        }
        case 3: { // silence
            const std::uint16_t samples = block.u16();
            const std::uint8_t rateByte = block.u8();
            VocPart part;
            part.rate = rateFromByte(rateByte);
            part.samples.assign(static_cast<std::size_t>(samples) + 1, 0x80);
            sound.parts.push_back(std::move(part));
            break;
        }
        case 5: { // text
            const auto data = block.bytes(block.remaining());
            sound.text.assign(data.begin(), data.end());
            break;
        }
        case 6: { // repeat start: the count is the extra plays, 0xFFFF forever
            const std::uint16_t count = block.u16();
            repeat = static_cast<int>(sound.parts.size());
            repeatPlays = count == 0xFFFF ? 0 : count + 1;
            break;
        }
        case 7: // repeat end
            if (repeat >= 0) {
                // Merge the repeated parts into one so each can carry a count.
                VocPart merged;
                for (std::size_t i = static_cast<std::size_t>(repeat); i < sound.parts.size(); ++i) {
                    if (merged.rate == 0) {
                        merged.rate = sound.parts[i].rate;
                    }
                    merged.samples.insert(merged.samples.end(), sound.parts[i].samples.begin(),
                                          sound.parts[i].samples.end());
                }
                sound.parts.resize(static_cast<std::size_t>(repeat));
                if (merged.rate != 0) {
                    merged.plays = repeatPlays;
                    sound.parts.push_back(std::move(merged));
                }
                repeat = -1;
            }
            break;
        default: // other blocks carry nothing the game needs
            break;
        }
    }
    return sound;
}

} // namespace mentat
