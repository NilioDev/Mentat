// SPDX-License-Identifier: GPL-3.0-or-later
#include "melange/data/hsq.hpp"

#include "melange/data/reader.hpp"

namespace melange {

namespace {

constexpr std::size_t kHeaderSize = 6;
constexpr std::uint8_t kHeaderSum = 0xAB;

// Hands out control bits from 16-bit words in the stream, lowest bit first.
class ControlBits {
public:
    explicit ControlBits(Reader& reader) : reader_(reader) {}

    unsigned next() {
        if (left_ == 0) {
            word_ = reader_.u16();
            left_ = 16;
        }
        const unsigned bit = word_ & 1u;
        word_ >>= 1;
        --left_;
        return bit;
    }

private:
    Reader& reader_;
    unsigned word_ = 0;
    int left_ = 0;
};

} // namespace

bool isHsq(std::span<const std::uint8_t> packed) {
    if (packed.size() < kHeaderSize) {
        return false;
    }
    unsigned sum = 0;
    for (std::size_t i = 0; i < kHeaderSize; ++i) {
        sum += packed[i];
    }
    return (sum & 0xFFu) == kHeaderSum;
}

std::vector<std::uint8_t> unpackHsq(std::span<const std::uint8_t> packed) {
    if (!isHsq(packed)) {
        throw FormatError("not an HSQ file");
    }
    Reader r(packed);
    const std::size_t unpackedSize = r.u16() | (static_cast<std::size_t>(r.u8()) << 16);
    r.skip(3);

    std::vector<std::uint8_t> out;
    out.reserve(unpackedSize);
    ControlBits bits(r);

    for (;;) {
        if (bits.next() == 1) {
            if (out.size() >= unpackedSize) {
                throw FormatError("HSQ stream makes more than its unpacked size");
            }
            out.push_back(r.u8());
            continue;
        }
        std::size_t length = 0;
        std::size_t distance = 0;
        if (bits.next() == 0) {
            const unsigned high = bits.next();
            const unsigned low = bits.next();
            length = ((high << 1) | low) + 2;
            distance = 256u - r.u8();
        } else {
            const std::uint16_t word = r.u16();
            distance = 8192u - (word >> 3);
            length = word & 7u;
            if (length == 0) {
                length = r.u8();
                if (length == 0) {
                    break;
                }
            }
            length += 2;
        }
        if (distance > out.size()) {
            throw FormatError("HSQ copy reaches before the start of the output");
        }
        if (length > unpackedSize - out.size()) {
            throw FormatError("HSQ stream makes more than its unpacked size");
        }
        const std::size_t from = out.size() - distance;
        for (std::size_t i = 0; i < length; ++i) {
            out.push_back(out[from + i]);
        }
    }
    if (out.size() != unpackedSize) {
        throw FormatError("HSQ stream ends before its unpacked size");
    }
    return out;
}

} // namespace melange
