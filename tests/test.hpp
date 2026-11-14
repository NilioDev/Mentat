// SPDX-License-Identifier: GPL-3.0-or-later
//
// A deliberately tiny test harness, so the core can be tested without
// third-party dependencies.
//
//   MLG_TEST(name) { MLG_CHECK(1 + 1 == 2); }
#pragma once

#include <functional>
#include <string_view>
#include <vector>

namespace melange::test {

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

} // namespace melange::test

#define MLG_TEST_CONCAT_INNER(a, b) a##b
#define MLG_TEST_CONCAT(a, b) MLG_TEST_CONCAT_INNER(a, b)

#define MLG_TEST(name)                                                                     \
    static void MLG_TEST_CONCAT(hgm_test_fn_, __LINE__)();                                 \
    static const ::melange::test::Registrar MLG_TEST_CONCAT(hgm_test_reg_, __LINE__){          \
        name, &MLG_TEST_CONCAT(hgm_test_fn_, __LINE__)};                                   \
    static void MLG_TEST_CONCAT(hgm_test_fn_, __LINE__)()

#define MLG_CHECK(...)                                                              \
    do {                                                                                   \
        if (!(__VA_ARGS__)) {                                                               \
            ::melange::test::reportFailure(__FILE__, __LINE__, #__VA_ARGS__);                   \
        }                                                                                  \
    } while (false)

// Ends the current test early and reports it as skipped, for tests that need
// files which are not always present (such as the original game data).
#define MLG_SKIP(reason)                                                                   \
    do {                                                                                   \
        ::melange::test::reportSkip(reason);                                                   \
        return;                                                                            \
    } while (false)
