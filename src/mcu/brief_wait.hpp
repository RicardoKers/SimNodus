// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A short busy wait before a thread blocks. The circuit and a board's
// firmware hand time to each other at every circuit step, and each side is
// usually ready within microseconds. Through a condition variable every
// handoff costs two thread wake-ups (tens of microseconds on Windows, several
// times the work of a step of a small circuit); looking at an atomic for a
// moment first costs a fraction of a microsecond. The wait only decides how
// the thread waits: what it waits for, and so the simulation's result, stays
// the same.
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace simnodus::mcu {

class BriefWait {
public:
    // True as soon as ready() is. False after `budget` without it, and at once
    // while the last waits ran out (the other side is slow or paused: after n
    // failures in a row the next 2^n - 1 waits, at most 63, are skipped), so a
    // thread that has to block anyway loses little. The caller then blocks as
    // it did before.
    template <typename Ready>
    bool wait(Ready&& ready, std::chrono::microseconds budget = std::chrono::microseconds(50))
    {
        if (ready()) {
            return true;
        }
        if (const int skip = skip_.load(std::memory_order_relaxed); skip > 0) {
            skip_.store(skip - 1, std::memory_order_relaxed);
            return false;
        }
        const auto deadline = std::chrono::steady_clock::now() + budget;
        do {
            for (int i = 0; i < 16; ++i) {
                relax();
                if (ready()) {
                    failures_.store(0, std::memory_order_relaxed);
                    return true;
                }
            }
        } while (std::chrono::steady_clock::now() < deadline);
        const int failures = std::min(failures_.load(std::memory_order_relaxed) + 1, 6);
        failures_.store(failures, std::memory_order_relaxed);
        skip_.store((1 << failures) - 1, std::memory_order_relaxed);
        return false;
    }

    // Waits that would be skipped now (for tests).
    int skipped() const { return skip_.load(std::memory_order_relaxed); }

private:
    static void relax()
    {
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
        _mm_pause();
#else
        std::this_thread::yield();
#endif
    }

    // Atomics: the waits of one object come from one thread at a time, but
    // not always from the same one.
    std::atomic<int> failures_{0};
    std::atomic<int> skip_{0};
};

} // namespace simnodus::mcu
