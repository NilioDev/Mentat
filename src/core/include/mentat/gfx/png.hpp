// SPDX-License-Identifier: GPL-3.0-or-later
//
// Writes a surface as a PNG file, for tests and developer tools. The file
// is stored without compression, so no zlib is needed.
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace mentat {

class Palette;
class Surface;

std::vector<std::uint8_t> encodePng(const Surface& surface, const Palette& palette);
void writePng(const std::filesystem::path& path, const Surface& surface, const Palette& palette);

} // namespace mentat
