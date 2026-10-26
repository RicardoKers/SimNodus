// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Buzzer sound (SN-228): a continuous tone at the frequency a running buzzer
// reports, through the Windows multimedia API (no extra libraries): a worker
// keeps a few short buffers queued on an open wave device, so a new frequency
// is heard within about 50 ms, without restarting the sound. Other platforms
// stay silent.
#pragma once

#include <atomic>
#include <thread>

namespace simnodus::ui {

class ToneOutput {
public:
    ToneOutput() = default;
    ToneOutput(const ToneOutput&) = delete;
    ToneOutput& operator=(const ToneOutput&) = delete;
    ~ToneOutput();

    // The tone to sound, in Hz; below 20 Hz: silence (faded out, the device closes after a while).
    void play(double hz);
    static bool available();

private:
    void run(); // the worker

    std::atomic<double> hz_{0.0};
    std::atomic<bool> quit_{false};
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace simnodus::ui
