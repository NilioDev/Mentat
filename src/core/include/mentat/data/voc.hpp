// SPDX-License-Identifier: GPL-3.0-or-later
//
// VOC sound files: the spoken voices and sound effects. See
// docs/formats/voc.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

// A stretch of sound at one sample rate, as unsigned 8-bit mono samples
// (0x80 is silence).
struct VocPart {
    int rate = 0;
    std::vector<std::uint8_t> samples;
    // How often the part is played: 1 normally, 0 for "until stopped".
    int plays = 1;
};

struct VocSound {
    std::vector<VocPart> parts;
    // The text block, when there is one. In the voices it holds the mouth
    // shapes for the speaking face.
    std::vector<std::uint8_t> text;

    // The length in seconds of one play of every part.
    double seconds() const;
};

class Voc {
public:
    // Takes the whole file. Throws FormatError.
    static VocSound parse(std::span<const std::uint8_t> bytes);

    // The sample rate for a VOC rate byte.
    static int rateFromByte(std::uint8_t value) { return 1000000 / (256 - value); }

    // Unpacks 4-bit packed samples (VOC codec 1). The first byte is the
    // starting sample.
    static std::vector<std::uint8_t> unpack4Bit(std::span<const std::uint8_t> data);
};

} // namespace mentat
