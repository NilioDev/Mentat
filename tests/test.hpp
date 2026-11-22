// SPDX-License-Identifier: GPL-3.0-or-later
//
// A deliberately tiny test harness, so the core can be tested without
// third-party dependencies.
//
//   MTT_TEST(name) { MTT_CHECK(1 + 1 == 2); }
#pragma once

#include <functional>
#include <string_view>
#include <vector>

namespace mentat::test {

struct Case {
    std::string_view name;
    std::function<void()> body;
};

std::vector<Case>& registry();
void reportFailure(const char* file, int line, const char* expression);
void reportSkip(const char* reason);

struct Registrar {
    Registrar(std::string_view name, std::function<void()> body) {
        registry().push_back({name, std::move(body)});
    }
};

} // namespace mentat::test

#define MTT_TEST_CONCAT_INNER(a, b) a##b
#define MTT_TEST_CONCAT(a, b) MTT_TEST_CONCAT_INNER(a, b)

#define MTT_TEST(name)                                                                     \
    static void MTT_TEST_CONCAT(hgm_test_fn_, __LINE__)();                                 \
    static const ::mentat::test::Registrar MTT_TEST_CONCAT(hgm_test_reg_, __LINE__){          \
        name, &MTT_TEST_CONCAT(hgm_test_fn_, __LINE__)};                                   \
    static void MTT_TEST_CONCAT(hgm_test_fn_, __LINE__)()

#define MTT_CHECK(...)                                                              \
    do {                                                                                   \
        if (!(__VA_ARGS__)) {                                                               \
            ::mentat::test::reportFailure(__FILE__, __LINE__, #__VA_ARGS__);                   \
        }                                                                                  \
    } while (false)

// Checks that an expression throws (any exception).
#define MTT_CHECK_THROWS(...)                                                              \
    do {                                                                                   \
        bool mttThrew = false;                                                             \
        try {                                                                              \
            static_cast<void>(__VA_ARGS__);                                                \
        } catch (...) {                                                                    \
            mttThrew = true;                                                               \
        }                                                                                  \
        if (!mttThrew) {                                                                   \
            ::mentat::test::reportFailure(__FILE__, __LINE__, "throws: " #__VA_ARGS__);    \
        }                                                                                  \
    } while (false)

// Ends the current test early and reports it as skipped, for tests that need
// files which are not always present (such as the original game data).
#define MTT_SKIP(reason)                                                                   \
    do {                                                                                   \
        ::mentat::test::reportSkip(reason);                                                   \
        return;                                                                            \
    } while (false)
