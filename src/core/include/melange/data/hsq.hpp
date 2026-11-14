// SPDX-License-Identifier: GPL-3.0-or-later
//
// HSQ compression, used by most files in DUNE.DAT. See docs/formats/hsq.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace melange {

// True when the bytes start with a valid HSQ header (the check byte adds up).
bool isHsq(std::span<const std::uint8_t> packed);

// Unpacks an HSQ file. Throws FormatError for a header that does not check
// out or a stream that is damaged.
std::vector<std::uint8_t> unpackHsq(std::span<const std::uint8_t> packed);

} // namespace melange
