// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "tone_output.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace simnodus::ui {

ToneOutput::~ToneOutput()
{
    quit_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool ToneOutput::available()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void ToneOutput::play(double hz)
{
    hz_ = hz < 20.0 ? 0.0 : std::min(hz, 12000.0);
#ifdef _WIN32
    if (hz_ > 0 && !running_) {
        if (worker_.joinable()) {
            worker_.join(); // a worker that closed its device after a silence
        }
        quit_ = false;
        running_ = true;
        worker_ = std::thread([this] { run(); });
    }
#endif
}

void ToneOutput::run()
{
#ifdef _WIN32
    constexpr int rate = 44100;
    constexpr int frames = 1024; // about 23 ms per buffer, four queued
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = rate * 2;
    HWAVEOUT device = nullptr;
    if (waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        running_ = false;
        return;
    }
    std::array<std::vector<std::int16_t>, 4> samples;
    std::array<WAVEHDR, 4> headers{};
    for (std::size_t i = 0; i < headers.size(); ++i) {
        samples[i].assign(frames, 0);
        headers[i].lpData = reinterpret_cast<LPSTR>(samples[i].data());
        headers[i].dwBufferLength = frames * 2;
        waveOutPrepareHeader(device, &headers[i], sizeof(WAVEHDR));
        headers[i].dwFlags |= WHDR_DONE; // free to fill
    }
    double phase = 0.0;    // continuous across buffers and frequency changes: no clicks
    double level = 0.0;    // faded in and out
    double frequency = 0.0;
    auto silent_since = std::chrono::steady_clock::now();
    while (!quit_) {
        bool wrote = false;
        for (std::size_t i = 0; i < headers.size(); ++i) {
            if (!(headers[i].dwFlags & WHDR_DONE)) {
                continue;
            }
            const double target = hz_;
            if (target > 0) {
                frequency = target;
            }
            for (int n = 0; n < frames; ++n) {
                level += ((target > 0 ? 1.0 : 0.0) - level) * 0.003;
                phase += 2.0 * 3.14159265358979 * frequency / rate;
                if (phase > 2.0 * 3.14159265358979) {
                    phase -= 2.0 * 3.14159265358979;
                }
                // A rounded square, like a buzzer.
                samples[i][static_cast<std::size_t>(n)] =
                    static_cast<std::int16_t>(std::lround(6000.0 * level * std::tanh(4.0 * std::sin(phase))));
            }
            headers[i].dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
            waveOutWrite(device, &headers[i], sizeof(WAVEHDR));
            wrote = true;
        }
        if (hz_ > 0 || level > 0.001) {
            silent_since = std::chrono::steady_clock::now();
        } else if (std::chrono::steady_clock::now() - silent_since > std::chrono::seconds(2)) {
            break; // quiet for a while: give the device back
        }
        if (!wrote) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    waveOutReset(device);
    for (auto& header : headers) {
        waveOutUnprepareHeader(device, &header, sizeof(WAVEHDR));
    }
    waveOutClose(device);
    running_ = false;
#endif
}

} // namespace simnodus::ui
