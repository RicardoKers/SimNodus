// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Minimal test registry: TEST_CASE registers a function, CHECK records a
// failure and continues, REQUIRE stops the current case.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace simnodus::test {

struct Case {
    const char* name;
    std::function<void()> body;
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

inline int& failures()
{
    static int count = 0;
    return count;
}

struct Register {
    Register(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

struct Abort {};

inline void fail(const char* file, int line, const std::string& what)
{
    ++failures();
    std::printf("  FAIL %s:%d: %s\n", file, line, what.c_str());
}

// Runs every case whose name contains `filter` (all when empty).
inline int run_all(const std::string& filter = {})
{
    int ran = 0;
    for (const auto& c : registry()) {
        if (!filter.empty() && std::string(c.name).find(filter) == std::string::npos) {
            continue;
        }
        const int before = failures();
        try {
            c.body();
        } catch (const Abort&) {
        } catch (const std::exception& error) {
            fail(__FILE__, __LINE__, std::string("exception: ") + error.what());
        }
        std::printf("%s %s\n", failures() == before ? "PASS" : "FAIL", c.name);
        ++ran;
    }
    std::printf("%d case(s), %d failure(s)\n", ran, failures());
    return failures() == 0 && ran > 0 ? 0 : 1;
}

} // namespace simnodus::test

#define SN_CONCAT_INNER(a, b) a##b
#define SN_CONCAT(a, b) SN_CONCAT_INNER(a, b)
#define TEST_CASE(name)                                                                                     \
    static void SN_CONCAT(test_body_, __LINE__)();                                                          \
    static const simnodus::test::Register SN_CONCAT(test_register_, __LINE__)(name, &SN_CONCAT(test_body_, __LINE__)); \
    static void SN_CONCAT(test_body_, __LINE__)()
#define CHECK(condition)                                                                                    \
    do {                                                                                                    \
        if (!(condition)) simnodus::test::fail(__FILE__, __LINE__, #condition);                             \
    } while (false)
#define REQUIRE(condition)                                                                                  \
    do {                                                                                                    \
        if (!(condition)) {                                                                                 \
            simnodus::test::fail(__FILE__, __LINE__, #condition);                                           \
            throw simnodus::test::Abort{};                                                                  \
        }                                                                                                   \
    } while (false)
