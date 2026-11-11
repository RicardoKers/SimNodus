// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "protocol_decode.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace simnodus::sim {

namespace {

std::string hex(unsigned value, int bits)
{
    char text[16];
    std::snprintf(text, sizeof text, "%0*X", std::max(2, (bits + 3) / 4), value);
    return text;
}

// The changes of several signals in time order: (time, signal index, level).
struct Event {
    double time = 0.0;
    int signal = 0;
    bool level = false;
};

std::vector<Event> merged(std::initializer_list<const LogicSignal*> signals)
{
    std::vector<Event> events;
    int index = 0;
    for (const auto* s : signals) {
        for (const auto& [time, level] : s->changes) {
            events.push_back({time, index, level});
        }
        ++index;
    }
    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.time < b.time; });
    return events;
}

} // namespace

bool level_at(const LogicSignal& signal, double time)
{
    const auto after = std::upper_bound(signal.changes.begin(), signal.changes.end(), time,
        [](double t, const std::pair<double, bool>& change) { return t < change.first; });
    return after == signal.changes.begin() ? signal.initial : std::prev(after)->second;
}

std::vector<DecodedItem> decode_i2c(const LogicSignal& scl, const LogicSignal& sda)
{
    std::vector<DecodedItem> items;
    bool clock = scl.initial;
    bool data = sda.initial;
    bool in_frame = false;
    bool first_byte = false;
    int bits = 0;
    unsigned value = 0;
    double byte_start = 0.0;
    for (const auto& e : merged({&scl, &sda})) {
        if (e.signal == 1) { // SDA
            const bool was = data;
            data = e.level;
            if (clock && was != data) {
                if (!data) { // falling while the clock is high: start
                    items.push_back({e.time, e.time, DecodedItem::Kind::Start, in_frame ? "Sr" : "S", 0, 0, true});
                    in_frame = true;
                    first_byte = true;
                    bits = 0;
                    value = 0;
                } else if (in_frame) { // rising while the clock is high: stop
                    items.push_back({e.time, e.time, DecodedItem::Kind::Stop, "P", 0, 0, true});
                    in_frame = false;
                }
            }
            continue;
        }
        const bool rising = !clock && e.level;
        clock = e.level;
        if (!rising || !in_frame) {
            continue;
        }
        if (bits == 0) {
            byte_start = e.time;
        }
        if (bits < 8) {
            value = (value << 1) | (data ? 1u : 0u);
            ++bits;
            continue;
        }
        // The ninth clock: the acknowledge bit (low).
        DecodedItem item;
        item.start = byte_start;
        item.end = e.time;
        item.acknowledged = !data;
        if (first_byte) {
            item.kind = DecodedItem::Kind::Address;
            item.value = value >> 1;
            item.text = hex(value >> 1, 8) + ((value & 1u) ? " R " : " W ") + (item.acknowledged ? "A" : "N");
        } else {
            item.kind = DecodedItem::Kind::Data;
            item.value = value;
            item.text = hex(value, 8) + (item.acknowledged ? " A" : " N");
        }
        items.push_back(item);
        first_byte = false;
        bits = 0;
        value = 0;
    }
    return items;
}

std::vector<DecodedItem> decode_spi(const LogicSignal& clock, const LogicSignal& mosi, const LogicSignal& miso,
    const LogicSignal& select, const SpiOptions& options)
{
    std::vector<DecodedItem> items;
    const int width = std::clamp(options.bits, 1, 32);
    // Modes 0 and 3 sample on the rising edge, 1 and 2 on the falling one.
    const bool sample_rising = ((options.mode & 2) != 0) == ((options.mode & 1) != 0);
    auto selected = [&](bool level) { return !options.use_select || level != options.select_active_low; };
    bool clk = clock.initial;
    bool out = mosi.initial;
    bool in = miso.initial;
    bool active = selected(select.initial);
    int bits = 0;
    unsigned out_word = 0;
    unsigned in_word = 0;
    double word_start = 0.0;
    static const LogicSignal none;
    for (const auto& e : merged({&clock, &mosi, options.has_miso ? &miso : &none, options.use_select ? &select : &none})) {
        switch (e.signal) {
        case 1:
            out = e.level;
            continue;
        case 2:
            in = e.level;
            continue;
        case 3:
            active = selected(e.level);
            bits = 0; // a word cut by the select line is dropped
            out_word = in_word = 0;
            continue;
        default:
            break;
        }
        const bool rising = !clk && e.level;
        const bool falling = clk && !e.level;
        clk = e.level;
        if (!active || !(sample_rising ? rising : falling)) {
            continue;
        }
        if (bits == 0) {
            word_start = e.time;
        }
        if (options.msb_first) {
            out_word = (out_word << 1) | (out ? 1u : 0u);
            in_word = (in_word << 1) | (in ? 1u : 0u);
        } else {
            out_word |= (out ? 1u : 0u) << bits;
            in_word |= (in ? 1u : 0u) << bits;
        }
        if (++bits < width) {
            continue;
        }
        DecodedItem item;
        item.start = word_start;
        item.end = e.time;
        item.kind = DecodedItem::Kind::Data;
        item.value = out_word;
        item.second = options.has_miso ? in_word : 0;
        item.text = hex(out_word, width) + (options.has_miso ? "/" + hex(in_word, width) : std::string());
        items.push_back(item);
        bits = 0;
        out_word = in_word = 0;
    }
    return items;
}

double uart_baud(const LogicSignal& line)
{
    std::vector<double> pulses;
    for (std::size_t i = 1; i < line.changes.size(); ++i) {
        const double pulse = line.changes[i].first - line.changes[i - 1].first;
        if (pulse > 0) {
            pulses.push_back(pulse);
        }
    }
    if (pulses.empty()) {
        return 0.0;
    }
    // The shortest pulse seen more than once: a single glitch (a pin that
    // becomes the transmitter's output) is not a bit.
    std::sort(pulses.begin(), pulses.end());
    double shortest = pulses.front();
    for (std::size_t i = 0; i + 1 < pulses.size(); ++i) {
        if (pulses[i + 1] <= pulses[i] * 1.15) {
            shortest = pulses[i];
            break;
        }
    }
    static const std::array<double, 15> rates{300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200,
        230400, 250000, 460800, 921600};
    const double measured = 1.0 / shortest;
    double best = rates.front();
    for (const double rate : rates) {
        if (std::abs(std::log(rate / measured)) < std::abs(std::log(best / measured))) {
            best = rate;
        }
    }
    return best;
}

std::vector<DecodedItem> decode_uart(const LogicSignal& line, const UartOptions& options)
{
    std::vector<DecodedItem> items;
    const double baud = options.baud > 0 ? options.baud : uart_baud(line);
    if (!(baud > 0)) {
        return items;
    }
    const double bit = 1.0 / baud;
    const int data_bits = std::clamp(options.data_bits, 5, 9);
    const bool parity = options.parity == 'E' || options.parity == 'O';
    const bool idle = !options.inverted; // the line's level between frames
    auto mark = [&](double time) { return level_at(line, time) == idle; };
    double free_from = -std::numeric_limits<double>::infinity(); // the line may start a frame from here
    bool level = line.initial;
    for (const auto& [time, next] : line.changes) {
        const bool was = level;
        level = next;
        if (time < free_from || was != idle || next == idle) {
            continue; // inside a frame, or not the edge that leaves the idle level
        }
        unsigned value = 0;
        int ones = 0;
        for (int i = 0; i < data_bits; ++i) {
            if (mark(time + (1.5 + i) * bit)) {
                value |= 1u << i;
                ++ones;
            }
        }
        bool parity_ok = true;
        if (parity) {
            ones += mark(time + (1.5 + data_bits) * bit) ? 1 : 0;
            parity_ok = (ones % 2 == 0) == (options.parity == 'E');
        }
        const double stop_at = time + (1.5 + data_bits + (parity ? 1 : 0)) * bit;
        DecodedItem item;
        item.start = time;
        item.end = stop_at + 0.5 * bit;
        item.value = value;
        if (!mark(stop_at)) {
            item.kind = DecodedItem::Kind::Error;
            item.text = "frame error";
            // The line is looked at again once it is back at its idle level.
            free_from = stop_at;
        } else {
            free_from = stop_at;
            if (!parity_ok) {
                item.kind = DecodedItem::Kind::Error;
                item.text = hex(value, data_bits) + " parity error";
            } else {
                item.kind = DecodedItem::Kind::Data;
                item.text = hex(value, data_bits);
                if (value >= 0x20 && value < 0x7F) {
                    item.text += std::string(" '") + static_cast<char>(value) + "'";
                }
            }
        }
        items.push_back(item);
    }
    return items;
}

} // namespace simnodus::sim
