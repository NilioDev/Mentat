// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/resources.hpp"

#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"

namespace mentat {

std::string Resources::name(int index) const {
    std::string n = resourceName(program_, index);
    if (n.empty()) {
        throw FormatError("resource " + std::to_string(index) + " is not used");
    }
    return n;
}

std::vector<std::uint8_t> Resources::load(int index) { return archive_.load(name(index)); }

const SpriteSheet& Resources::sheet(int index) {
    auto& slot = sheets_[index];
    if (!slot) {
        slot = std::make_unique<SpriteSheet>(SpriteSheet::parse(load(index)));
    }
    return *slot;
}

} // namespace mentat
