// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Buzzer sound (SN-228): a looping tone at the frequency a running buzzer
// reports, through the Windows multimedia API (no extra libraries). Other
// platforms stay silent.
#pragma once

#include <vector>

namespace simnodus::ui {

class ToneOutput {
public:
    ToneOutput() = default;
    ToneOutput(const ToneOutput&) = delete;
    ToneOutput& operator=(const ToneOutput&) = delete;
    ~ToneOutput();

    // Plays a tone of `hz` (silence below 20 Hz); a change under 2 % keeps the current tone.
    void play(double hz);
    static bool available();

private:
    void stop();

    double playing_ = 0.0;
    std::vector<char> wave_; // the WAV image the system loops; alive while it plays
};

} // namespace simnodus::ui
