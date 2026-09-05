// Copyright (C) 2026, Lux Industries Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Eco
//
// check.hpp — the whole test harness. A failing check prints where and what,
// and the process exit code is the verdict.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace zaptest {

inline int failures = 0;

inline void report(const char* file, int line, const char* what) {
    std::printf("FAIL %s:%d  %s\n", file, line, what);
    ++failures;
}

inline std::string hex(std::span<const std::uint8_t> b) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(b.size() * 2);
    for (std::uint8_t c : b) {
        s.push_back(d[c >> 4]);
        s.push_back(d[c & 0xf]);
    }
    return s;
}

inline std::vector<std::uint8_t> unhex(std::string_view s) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(nib(s[i]) * 16 + nib(s[i + 1])));
    return out;
}

inline int verdict(const char* suite) {
    if (failures == 0) {
        std::printf("ok   %s\n", suite);
        return 0;
    }
    std::printf("FAIL %s: %d check(s)\n", suite, failures);
    return 1;
}

}  // namespace zaptest

#define CHECK(cond)                                            \
    do {                                                       \
        if (!(cond)) zaptest::report(__FILE__, __LINE__, #cond); \
    } while (0)

#define CHECK_EQ(got, want)                                                      \
    do {                                                                         \
        auto _g = (got);                                                         \
        auto _w = (want);                                                        \
        if (!(_g == _w))                                                         \
            zaptest::report(__FILE__, __LINE__, #got " != " #want);              \
    } while (0)

#define CHECK_HEX(got, want)                                                     \
    do {                                                                         \
        const std::string _g = zaptest::hex(got);                                \
        const std::string _w = (want);                                           \
        if (_g != _w) {                                                          \
            std::printf("  got  %s\n  want %s\n", _g.c_str(), _w.c_str());       \
            zaptest::report(__FILE__, __LINE__, "bytes differ");                 \
        }                                                                        \
    } while (0)
