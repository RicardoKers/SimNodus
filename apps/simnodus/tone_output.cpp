// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "tone_output.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace simnodus::ui {
#ifdef _WIN32 // the tone is only built where it can be played
namespace {

template <typename T>
void put(std::vector<char>& out, T value)
{
    char bytes[sizeof(T)];
    std::memcpy(bytes, &value, sizeof(T));
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

void put_tag(std::vector<char>& out, const char* tag) { out.insert(out.end(), tag, tag + 4); }

// A WAV image of whole cycles of a buzzer-like tone (a square wave with soft
// edges), about a quarter of a second long, so it loops without a click.
std::vector<char> tone_wave(double hz)
{
    constexpr int rate = 44100;
    const int cycles = std::max(1, static_cast<int>(std::lround(hz * 0.25)));
    const int samples = static_cast<int>(std::lround(cycles * rate / hz));
    std::vector<char> out;
    const auto data_bytes = static_cast<std::uint32_t>(samples * 2);
    put_tag(out, "RIFF");
    put<std::uint32_t>(out, 36 + data_bytes);
    put_tag(out, "WAVE");
    put_tag(out, "fmt ");
    put<std::uint32_t>(out, 16);
    put<std::uint16_t>(out, 1); // PCM
    put<std::uint16_t>(out, 1); // mono
    put<std::uint32_t>(out, rate);
    put<std::uint32_t>(out, rate * 2);
    put<std::uint16_t>(out, 2);
    put<std::uint16_t>(out, 16);
    put_tag(out, "data");
    put<std::uint32_t>(out, data_bytes);
    const double step = 2.0 * 3.14159265358979 * cycles / samples;
    for (int i = 0; i < samples; ++i) {
        const double s = std::tanh(4.0 * std::sin(step * i)); // a rounded square, like a buzzer
        put<std::int16_t>(out, static_cast<std::int16_t>(std::lround(6000.0 * s)));
    }
    return out;
}

} // namespace
#endif

ToneOutput::~ToneOutput()
{
    stop();
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
    if (hz < 20.0) {
        stop();
        return;
    }
    hz = std::min(hz, 12000.0);
    if (playing_ > 0 && std::abs(hz - playing_) < 0.02 * playing_) {
        return;
    }
#ifdef _WIN32
    stop(); // the system must let go of the old image first
    wave_ = tone_wave(hz);
    PlaySoundA(wave_.data(), nullptr, SND_MEMORY | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
#endif
    playing_ = hz;
}

void ToneOutput::stop()
{
    if (playing_ <= 0) {
        return;
    }
#ifdef _WIN32
    PlaySoundA(nullptr, nullptr, 0);
#endif
    playing_ = 0;
    wave_.clear();
}

} // namespace simnodus::ui
