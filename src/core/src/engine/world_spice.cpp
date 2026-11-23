// SPDX-License-Identifier: GPL-3.0-or-later
//
// The spice the Emperor asks for: what Duncan proposes to send, Paul's
// answer, the shipment from the message room and the Emperor's reply. See
// docs/rules/shipments.md.
#include "mentat/engine/world.hpp"

namespace mentat {

namespace {

constexpr std::size_t kClockWord = 0x02;
constexpr std::size_t kWith = 0x10;
constexpr std::size_t kAnswers = 0x1A;  // how many answers Paul gave in this talk
constexpr std::size_t kBill = 0x20;     // the bill of the smuggler spoken of
constexpr std::size_t kBills = 0x22;    // the bills not paid
constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kAnswer = 0x9F;   // 1 accept, 2 refuse, 3 argue
constexpr std::size_t kSpice = 0xA0;
constexpr std::size_t kProposals = 0xB4; // four words
constexpr std::size_t kDemand = 0xBC;
constexpr std::size_t kMood = 0xBE;
constexpr std::size_t kEmperor = 0xBF;
constexpr std::size_t kToSend = 0xC0;
constexpr std::size_t kDemands = 0xC3;
constexpr std::size_t kDaysLeft = 0xCF;
constexpr std::size_t kMarkedPlace = 0xFF7;
constexpr std::size_t kSmugglers = 0x10D8;
constexpr std::size_t kSmugglerSize = 0x11;
constexpr std::size_t kSmugglerNow = 0x10B4;
constexpr std::size_t kLastBill = 0x113F;
constexpr std::size_t kSlots = 0x1152;
constexpr std::size_t kShipmentDue = 0x1158;
constexpr std::size_t kSentToday = 0x1172;
constexpr std::size_t kNextDemand = 0x118D;
constexpr std::size_t kLastWarning = 0x11BB;
constexpr std::size_t kVillageName = 0x11F7;
constexpr std::size_t kCharacterTable = 0xFD8;
constexpr std::size_t kCharacterSize = 0x10;

std::uint16_t word(unsigned v) { return static_cast<std::uint16_t>(v & 0xFFFF); }

} // namespace

int World::emperorsDispleasure() const {
    const int mood = u8(kMood);
    return (mood < 1) + (mood < 0x40) + (mood < 0x80) + (mood < 0x90) + (mood < 0xFF);
}

void World::smugglerNow(std::size_t s) {
    setU16(kSmugglerNow, static_cast<std::uint16_t>(s));
    setU8(0x1C, u8(s + 2));
    setU16(kBill, u16(s + 0x0E));
    setU8(0x1F, 0);
    if (u16(s + 0x0E) != 0) {
        setU8(0x1F, static_cast<std::uint8_t>((u16(kClockWord) >> 4) - u8(s + 0x10)));
    }
    setU8(0x1D, u8(s + 1));
}

void World::prepareShipment() {
    setU8(kAnswer, 0);
    setU16(kBill, 0);
    setU8(kAnswers, 0);
    const unsigned stock = u16(kSpice);
    if (stock != 0) {
        setU8(kAnswer, 3);
    }

    // Four amounts to propose, by the stock and the demand.
    const unsigned demand = u16(kDemand);
    const unsigned half = stock >> 1;
    const unsigned threeQuarters = (stock >> 2) + half;
    const unsigned more = word((demand >> 1) + demand);
    const unsigned twice = word(demand + demand);
    std::uint8_t emperor = static_cast<std::uint8_t>(u8(kEmperor) & ~0x06);
    unsigned p[4];
    if (stock < demand) {
        p[0] = stock;
        p[1] = threeQuarters;
        p[2] = half;
        p[3] = word(threeQuarters - half);
    } else {
        p[0] = demand;
        p[1] = stock;
        if (stock < more) {
            p[2] = threeQuarters;
            p[3] = half;
            emperor |= 0x02;
        } else if (stock < twice) {
            p[2] = threeQuarters;
            p[3] = more;
            emperor |= 0x04;
        } else {
            p[2] = more;
            p[3] = twice;
            emperor |= 0x06;
        }
    }
    setU8(kEmperor, emperor);
    for (std::size_t i = 0; i < 4; ++i) {
        setU16(kProposals + 2 * i, word(p[i]));
    }

    // A smuggler's bill: the one waiting longest that was neither refused
    // nor argued over; else the next one with a bill after the last.
    const std::uint8_t day = static_cast<std::uint8_t>(u16(kClockWord) >> 4);
    std::size_t chosen = 0;
    std::uint8_t longest = 0;
    for (std::size_t s = kSmugglers; s + kSmugglerSize <= kSize && u8(s) != 0xFF; s += kSmugglerSize) {
        if (u16(s + 0x0E) == 0 || (u8(s + 2) & 0x60)) {
            continue;
        }
        const auto waited = static_cast<std::uint8_t>(day - u8(s + 0x10));
        if (waited > longest) {
            longest = waited;
            chosen = s;
        }
    }
    if (chosen == 0) {
        const std::size_t last = u16(kLastBill);
        if (last < kSmugglers || last >= kSize) {
            return;
        }
        std::size_t s = last;
        for (;;) {
            s += kSmugglerSize;
            if (s + kSmugglerSize > kSize || u8(s) == 0xFF) {
                s = kSmugglers;
            }
            if (u16(s + 0x0E) != 0) {
                setU16(kLastBill, static_cast<std::uint16_t>(s));
                chosen = s;
                break;
            }
            if (s == last) {
                return;
            }
        }
    }
    smugglerNow(chosen);
    setU16(kVillageName, u8(chosen));
}

void World::decideShipment(bool bill) {
    const int answer = u8(kAnswer);
    const std::size_t s = u16(kSmugglerNow);
    if (answer == 2 || answer > 2) {
        // Refused (bit 6) or argued over (bit 5): the bill waits.
        if (bill && s >= kSmugglers && s + kSmugglerSize <= kSize) {
            setU8(s + 2, static_cast<std::uint8_t>((u8(s + 2) & 0x9F) | (answer == 2 ? 0x40 : 0x20)));
        }
        return;
    }
    if (!bill) {
        const std::size_t which = static_cast<std::size_t>((u8(kAnswers) - 1) & 3);
        setU16(kToSend, u16(kProposals + 2 * which));
        setU16(kShipmentDue, 0xFFFF);
        return;
    }
    if (s < kSmugglers || s + kSmugglerSize > kSize) {
        return;
    }
    const std::uint16_t amount = u16(s + 0x0E);
    setU16(s + 0x0E, 0);
    setU8(kBills, static_cast<std::uint8_t>(u8(kBills) - 1));
    setU16(kSpice, word(u16(kSpice) - amount));
    setU16(kSentToday, word(u16(kSentToday) + amount));
}

bool World::emperorReplies() {
    if (u8(kProgress) < 0x10) {
        setU8(kMarkedPlace, static_cast<std::uint8_t>(u8(kMarkedPlace) | 0x10));
        return false;
    }
    setU16(kToSend, 0);
    setU8(kEmperor, static_cast<std::uint8_t>(u8(kEmperor) | 0x01));
    const int about = emperorsDispleasure() + 7;
    if (about == 0x0C) {
        setU8(kLastWarning, static_cast<std::uint8_t>(u8(kLastWarning) + 1));
    }
    sendNews(static_cast<std::uint16_t>((about << 8) | 0x0B));
    return true;
}

void World::sendShipment(unsigned amount) {
    constexpr int kDuncan = 3;
    const std::size_t duncan = kCharacterTable + kCharacterSize * kDuncan;
    if ((u8(duncan + 0x0F) & 0x40) && (u8(duncan + 0x0F) & 0x02)) {
        stopsFollowing(kDuncan);
    }
    setU16(kSpice, word(u16(kSpice) - amount));
    setU16(kSentToday, word(u16(kSentToday) + amount));

    // The mood: 0x80 when what was asked was sent, in proportion to it
    // (at most 0xFF), and never 0.
    const unsigned demand = u16(kDemand);
    unsigned ratio = demand == 0 ? 0x1FF : ((amount & 0xFFFF) << 8) / demand;
    if (ratio >= 0x200) {
        ratio = 0x1FF;
    }
    int mood = static_cast<int>(ratio >> 1);
    if (mood == 0) {
        mood = 1;
    }
    std::uint8_t emperor = 0;
    int days = 0;
    if (mood >= 0xC0) {
        emperor = 0x40;
        days = 7;
    } else if (mood > 0x80) {
        emperor = 0x40;
        days = 6;
    } else if (mood == 0x80) {
        days = 5;
    } else {
        days = 4;
        emperor = 0x08;
        if (u8(kEmperor) & 0x08) {
            mood = 0; // short twice running
        }
    }
    setU8(kMood, static_cast<std::uint8_t>(mood));
    setU8(kEmperor, static_cast<std::uint8_t>(emperor | 0x80));
    unsigned next = u16(kNextDemand) + static_cast<unsigned>(days);
    next += static_cast<unsigned>(dice((u8(kDemands) >> 1) & 3));
    setU16(kNextDemand, word(next));
    setU8(kDaysLeft, static_cast<std::uint8_t>(next - (u16(kClockWord) >> 4)));
    setU16(kShipmentDue, 0);
}

void World::stopsFollowing(int c) {
    const std::size_t r = kCharacterTable + kCharacterSize * static_cast<std::size_t>(c);
    setU8(r + 0x0F, static_cast<std::uint8_t>(u8(r + 0x0F) & ~0x40));
    if (word(u16(kClockWord) - u16(r + 0x08)) >= 2) {
        setU16(r + 0x0A, u16(kClockWord));
    }
    const int number = u8(r + 0x0E);
    setU16(kWith, static_cast<std::uint16_t>(u16(kWith) & ~(1u << (number & 15))));
    if (u8(kSlots + 1) == number) {
        setU8(kSlots + 1, 0xFF);
    } else if (u8(kSlots) == number) {
        setU8(kSlots, u8(kSlots + 1));
        setU8(kSlots + 1, 0xFF);
    }
}

} // namespace mentat
