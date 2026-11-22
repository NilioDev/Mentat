// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/audio/music.hpp"

#include "mentat/data/dat_archive.hpp"
#include "mentat/data/reader.hpp"

namespace mentat {

namespace {

constexpr std::size_t kHeader = 2;          // voice offsets count from here
constexpr std::size_t kRepeatStart = 2 + 42; // 0x2C
constexpr std::size_t kFirstEvents = 2 + 50; // first byte after the header

} // namespace

Song Song::parse(std::vector<std::uint8_t> bytes) {
    Reader r(bytes);
    Song song;
    song.instruments_ = r.u16();
    std::array<std::uint16_t, kVoices> offsets{};
    for (auto& offset : offsets) {
        offset = r.u16();
    }
    r.seek(kRepeatStart);
    song.repeatStart_ = r.u16();
    song.repeatEnd_ = r.u16();
    song.repeatCount_ = r.u16();
    song.tempo_ = r.u16();
    if (song.instruments_ < kFirstEvents || song.instruments_ > bytes.size()) {
        throw FormatError("music: bad instrument offset");
    }
    song.instrumentCount_ = (bytes.size() - song.instruments_) / kInstrumentSize;
    for (int v = 0; v < kVoices; ++v) {
        const std::size_t offset = offsets[static_cast<std::size_t>(v)];
        if (offset == 0) {
            continue;
        }
        const std::size_t start = kHeader + offset;
        if (start < kFirstEvents || start >= song.instruments_) {
            throw FormatError("music: voice outside the event lists");
        }
        song.voices_[static_cast<std::size_t>(v)] = start;
    }
    song.bytes_ = std::move(bytes);
    return song;
}

std::span<const std::uint8_t> Song::instrument(std::size_t index) const {
    if (index >= instrumentCount_) {
        throw FormatError("music: no such instrument");
    }
    return std::span<const std::uint8_t>(bytes_).subspan(instruments_ + index * kInstrumentSize, kInstrumentSize);
}

MusicTables MusicTables::fromPlayer(std::span<const std::uint8_t> player) {
    MusicTables t;
    t.card = MusicCard::SoundBlasterPro;
    Reader r(player);
    r.seek(0x47);
    for (auto& f : t.frequency) {
        f = r.u16();
    }
    for (int i = 0; i < kAdLibVoices; ++i) {
        t.firstFrequency[static_cast<std::size_t>(i)] = r.u16();
    }
    r.seek(0x71);
    for (int i = 0; i < kAdLibVoices; ++i) {
        t.firstOperator[static_cast<std::size_t>(i)] = r.u8();
    }
    for (int i = 0; i < kAdLibVoices; ++i) {
        t.secondOperator[static_cast<std::size_t>(i)] = r.u8();
    }
    for (auto& d : t.distance) {
        d = r.u8();
    }
    r.seek(0x90);
    for (auto& f : t.fineLow) {
        f = r.u8();
    }
    for (auto& f : t.fineHigh) {
        f = r.u8();
    }
    for (int i = 0; i < kAdLibVoices; ++i) {
        const auto v = static_cast<std::size_t>(i);
        t.channel[v] = static_cast<std::uint8_t>(i);
        t.pairedChannel[v] = static_cast<std::uint8_t>(i);
        if (t.firstOperator[v] > 0x15 || t.secondOperator[v] > 0x15) {
            throw FormatError("music player: bad operator table");
        }
    }
    return t;
}

MusicTables MusicTables::fromGoldPlayer(std::span<const std::uint8_t> player) {
    MusicTables t;
    t.card = MusicCard::AdLibGold;
    Reader r(player);
    r.seek(0x42);
    for (auto& f : t.frequency) {
        f = r.u16();
    }
    for (auto& f : t.firstFrequency) {
        f = r.u16();
    }
    r.seek(0x7F);
    for (auto* table : {&t.channel, &t.pairedChannel, &t.firstOperator, &t.secondOperator}) {
        for (auto& b : *table) {
            b = r.u8();
        }
    }
    for (auto& d : t.distance) {
        d = r.u8();
    }
    for (auto& f : t.fineLow) {
        f = r.u8();
    }
    for (auto& f : t.fineHigh) {
        f = r.u8();
    }
    for (std::size_t v = 0; v < Song::kVoices; ++v) {
        if ((t.channel[v] & 0x7F) > 8 || (t.firstOperator[v] & 0x7F) > 0x15 || (t.secondOperator[v] & 0x7F) > 0x15) {
            throw FormatError("music player: bad channel table");
        }
    }
    return t;
}

MusicTables MusicTables::load(DatArchive& archive, MusicCard card) {
    if (card == MusicCard::AdLibGold && archive.directory().find("DNADG.HSQ") != nullptr) {
        return fromGoldPlayer(archive.load("DNADG.HSQ"));
    }
    return fromPlayer(archive.load("DNADP.HSQ"));
}

} // namespace mentat
