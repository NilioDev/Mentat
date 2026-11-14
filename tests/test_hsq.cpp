// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "melange/data/hsq.hpp"
#include "melange/data/reader.hpp"

#include <cstdint>
#include <vector>

using namespace melange;

namespace {

// "ABABAB": two literals, then a short copy of 4 bytes from 2 back, then the
// end marker. Control word 0x0093 holds the bits 1 1 0 0 1 0 0 1.
const std::vector<std::uint8_t> kAbab = {
    0x06, 0x00, 0x00, 0x0E, 0x00, 0x97,             // header, sums to 0xAB
    0x93, 0x00, 'A', 'B', 0xFE, 0x00, 0x00, 0x00,   // stream
};

bool throwsFormatError(const std::vector<std::uint8_t>& bytes) {
    try {
        unpackHsq(bytes);
    } catch (const FormatError&) {
        return true;
    }
    return false;
}

} // namespace

MLG_TEST("hsq: recognises the header by its check byte") {
    MLG_CHECK(isHsq(kAbab));
    auto broken = kAbab;
    broken[5] = 0;
    MLG_CHECK(!isHsq(broken));
    MLG_CHECK(!isHsq(std::vector<std::uint8_t>{0xAB}));
}

MLG_TEST("hsq: unpacks literals and an overlapping short copy") {
    const auto out = unpackHsq(kAbab);
    const std::vector<std::uint8_t> expected = {'A', 'B', 'A', 'B', 'A', 'B'};
    MLG_CHECK(out == expected);
}

MLG_TEST("hsq: unpacks a long copy") {
    // Literal 'X', then a long copy of 2 + 3 bytes from 1 back: word
    // ((8192 - 1) << 3) | 3 = 0xFFFB. Then the end marker. Bits: 1 0 1 0 1.
    const std::vector<std::uint8_t> packed = {
        0x06, 0x00, 0x00, 0x0F, 0x00, 0x96,
        0x15, 0x00, 'X', 0xFB, 0xFF, 0x00, 0x00, 0x00, 0x00,
    };
    MLG_CHECK(isHsq(packed));
    const auto out = unpackHsq(packed);
    MLG_CHECK(out == std::vector<std::uint8_t>(6, 'X'));
}

MLG_TEST("hsq: refuses a damaged stream") {
    auto truncated = kAbab;
    truncated.resize(9);
    MLG_CHECK(throwsFormatError(truncated));

    auto tooFarBack = kAbab;
    tooFarBack[10] = 0x00; // distance 256, only 2 bytes written
    MLG_CHECK(throwsFormatError(tooFarBack));

    auto tooLong = kAbab;
    tooLong[0] = 0x05;     // header says 5 bytes, stream makes 6
    tooLong[5] = 0x98;     // keep the check byte right
    MLG_CHECK(throwsFormatError(tooLong));

    auto tooShort = kAbab;
    tooShort[0] = 0x07;
    tooShort[5] = 0x96;
    MLG_CHECK(throwsFormatError(tooShort));
}
