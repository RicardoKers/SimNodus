// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Example firmware in Renode and in the SimNodus emulator (E-07, gate G0b.1):
// the same ELF with the same scripted inputs must give the same output pin
// transitions and UART bytes. Renode reports events on its 100 us quantum, so
// times agree within 200 us, plus Renode's documented SysTick deviation: it
// counts RVR instead of RVR + 1 cycles per period (ADR 0083), so it runs fast
// by 1/(RVR + 1): 125 ppm with RVR 7999, 250 ppm with RVR 3999 (lesson 1). The
// ARMv7-M manual (B3.3) and the emulator count RVR + 1. The tolerance allows
// 300 ppm, enough for RVR >= 3999 as in these examples. Events in the last
// millisecond are not compared, and one extra edge within the tolerance of the
// end is ignored (the drift moves edges across it). The sequences must be equal.
//
//   emu_firmware_compare <renode> <renode-resources> <examples-dir> [filter]
#include "emu/stm32f103.hpp"
#include "mcu/renode_engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace simnodus;
namespace fs = std::filesystem;

namespace {

struct Input {
    std::uint64_t time_ms = 0;
    int port = 0;
    int pin = 0;
    bool level = false;
    std::string uart; // bytes for USART1 instead of a pin level
};

struct Case {
    const char* firmware;
    std::uint64_t run_ms;
    std::vector<std::pair<int, int>> outputs; // compared pins (port, pin)
    std::vector<Input> inputs;
    // Renode starts SysTick with a full 2^24-cycle period when it is enabled
    // with CVR = 0 (the ARMv7-M manual, B3.3, loads RVR on the next clock), so
    // firmware that enables SysTick that way (FreeRTOS) ticks about 2.1 s late
    // at 8 MHz in Renode. For such cases Renode runs `renode_extra_ms` longer and
    // its events after `shift_after_us` are moved back by the measured offset.
    std::uint64_t renode_extra_ms = 0;
    std::uint64_t shift_after_us = 0;
};

struct Transition {
    std::uint64_t time_us;
    bool level;
};

struct Trace {
    std::map<std::pair<int, int>, std::vector<Transition>> pins;
    std::vector<std::pair<std::uint64_t, std::uint8_t>> uart;
};

const char* port_name(int port)
{
    static const char* names[] = {"gpioPortA", "gpioPortB", "gpioPortC", "gpioPortD", "gpioPortE"};
    return names[port];
}

// Keeps only level changes, starting from low (both sides start with all pins low).
void add_transition(Trace& trace, int port, int pin, std::uint64_t time_us, bool level)
{
    auto& list = trace.pins[{port, pin}];
    const bool previous = list.empty() ? false : list.back().level;
    if (level != previous) {
        list.push_back({time_us, level});
    }
}

Trace run_emulator(const Case& c, const fs::path& elf, std::string& error)
{
    Trace trace;
    const auto image = emu::read_elf(elf, error);
    emu::Stm32F103 m;
    if (!image || !m.load(*image, error)) {
        return trace;
    }
    for (std::uint64_t ms = 0; ms < c.run_ms; ++ms) {
        for (const auto& in : c.inputs) {
            if (in.time_ms == ms) {
                if (in.uart.empty()) {
                    m.set_pin_input(in.port, in.pin, in.level);
                } else {
                    for (const char ch : in.uart) {
                        m.uart_receive(1, static_cast<std::uint8_t>(ch));
                    }
                }
            }
        }
        m.run_until_ns((ms + 1) * 1000000);
    }
    for (const auto& e : m.take_pin_events()) {
        for (const auto& [port, pin] : c.outputs) {
            if (e.port == port && e.pin == pin) {
                add_transition(trace, port, pin, e.time_ns / 1000, e.level);
            }
        }
    }
    for (const auto& b : m.take_uart_bytes()) {
        if (b.usart == 1) {
            trace.uart.emplace_back(b.time_ns / 1000, b.value);
        }
    }
    return trace;
}

Trace run_renode(const Case& c, const fs::path& renode, const fs::path& resources, const fs::path& elf, std::string& error)
{
    Trace trace;
    mcu::MachineOptions options;
    options.renode = renode;
    options.resources = resources;
    options.firmware = elf;
    options.after_firmware = {"sysbus.cpu VectorTableOffset 0x08000000", "emulation SetGlobalAdvanceImmediately true"};
    options.start_timeout = std::chrono::milliseconds(180000);
    auto engine = mcu::RenodeEngine::start(options, error);
    if (!engine) {
        return trace;
    }
    for (const auto& [port, pin] : c.outputs) {
        engine->watch_gpio(port_name(port), pin);
    }
    engine->watch_uart("usart1");
    for (std::uint64_t ms = 0; ms < c.run_ms + c.renode_extra_ms; ++ms) {
        for (const auto& in : c.inputs) {
            if (in.time_ms == ms) {
                if (in.uart.empty()) {
                    engine->set_gpio(port_name(in.port), in.pin, in.level);
                } else {
                    engine->write_uart("usart1", std::vector<std::uint8_t>(in.uart.begin(), in.uart.end()));
                }
            }
        }
        if (!engine->run_for(1000)) {
            error = "Renode: " + engine->error();
            return trace;
        }
    }
    engine->run_for(1000); // events Renode raises after a RunFor arrive with the next one (ADR 0085)
    for (const auto& e : engine->take_gpio_events()) {
        if (e.time_us > (c.run_ms + c.renode_extra_ms) * 1000) {
            continue;
        }
        for (int port = 0; port < 5; ++port) {
            if (e.port == port_name(port)) {
                add_transition(trace, port, e.pin, e.time_us, e.state);
            }
        }
    }
    for (const auto& b : engine->take_uart_bytes()) {
        if (b.time_us <= (c.run_ms + c.renode_extra_ms) * 1000) {
            trace.uart.emplace_back(b.time_us, b.value);
        }
    }
    engine->stop();
    return trace;
}

std::uint64_t distance(std::uint64_t a, std::uint64_t b)
{
    return a > b ? a - b : b - a;
}

// Allowed difference at time `t_us`: two Renode quanta plus the SysTick drift.
std::uint64_t tolerance(std::uint64_t t_us)
{
    return 200 + t_us * 300 / 1000000;
}

void trim(Trace& trace, std::uint64_t end_us)
{
    for (auto& [pin, list] : trace.pins) {
        while (!list.empty() && list.back().time_us >= end_us) {
            list.pop_back();
        }
    }
    while (!trace.uart.empty() && trace.uart.back().first >= end_us) {
        trace.uart.pop_back();
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::printf("usage: emu_firmware_compare <renode> <renode-resources> <examples-dir> [filter]\n");
        return 2;
    }
    const fs::path renode = argv[1];
    const fs::path resources = argv[2];
    const fs::path examples = argv[3];
    const std::string filter = argc > 4 ? argv[4] : "";
    const std::vector<Case> cases = {
        {"blink", 2100, {{2, 13}, {0, 0}, {0, 1}, {0, 2}, {0, 3}}, {}},
        {"button_irq", 400, {{0, 0}, {2, 13}}, {{0, 0, 1, true, ""}, {100, 0, 1, false, ""}, {200, 0, 1, true, ""}, {300, 0, 1, false, ""}}},
        {"uart_echo", 1300, {{0, 0}}, {{1050, 0, 0, false, "1"}, {1150, 0, 0, false, "0hello\r"}}},
        {"lesson1_gpio", 30, {{0, 0}, {0, 1}}, {}},
        {"lesson2_button", 400, {{0, 0}}, {{0, 0, 1, true, ""}, {100, 0, 1, false, ""}, {101, 0, 1, true, ""}, {102, 0, 1, false, ""}, {250, 0, 1, true, ""}}},
        {"hal_clock", 1100, {{2, 13}, {0, 1}}, {}},
        // FreeRTOS: task switches on PB0-PB2 (G0b.3), blinking tasks on PA0/PA1, queue to USART1.
        {"freertos_tasks", 1000, {{0, 0}, {0, 1}, {1, 0}, {1, 1}, {1, 2}}, {}, 2200, 10000},
    };
    int failures = 0;
    for (const auto& c : cases) {
        if (!filter.empty() && std::string(c.firmware).find(filter) == std::string::npos) {
            continue;
        }
        const fs::path elf = examples / "firmware" / c.firmware / (std::string(c.firmware) + ".elf");
        std::string error;
        Trace ours = run_emulator(c, elf, error);
        if (!error.empty()) {
            std::printf("%s: emulator: %s\n", c.firmware, error.c_str());
            ++failures;
            continue;
        }
        Trace theirs = run_renode(c, renode, resources, elf, error);
        if (!error.empty()) {
            std::printf("%s: %s\n", c.firmware, error.c_str());
            ++failures;
            continue;
        }
        if (c.shift_after_us != 0) {
            // Offset from the first output change after `shift_after_us` on the first pin.
            const auto key = c.outputs.front();
            std::uint64_t mine = 0;
            std::uint64_t other = 0;
            for (const auto& tr : ours.pins[key]) {
                if (tr.time_us > c.shift_after_us) {
                    mine = tr.time_us;
                    break;
                }
            }
            for (const auto& tr : theirs.pins[key]) {
                if (tr.time_us > c.shift_after_us) {
                    other = tr.time_us;
                    break;
                }
            }
            const std::uint64_t offset = other > mine ? other - mine : 0;
            std::printf("  %s: Renode's first SysTick period is %.1f ms late (2^24 cycles; ADR 0089 report)\n",
                c.firmware, static_cast<double>(offset) / 1000.0);
            for (auto& [pin, list] : theirs.pins) {
                for (auto& tr : list) {
                    if (tr.time_us > c.shift_after_us) {
                        tr.time_us -= offset;
                    }
                }
            }
            for (auto& b : theirs.uart) {
                if (b.first > c.shift_after_us) {
                    b.first -= offset;
                }
            }
        }
        trim(ours, (c.run_ms - 1) * 1000);
        trim(theirs, (c.run_ms - 1) * 1000);
        int problems = 0;
        std::uint64_t worst = 0;
        std::size_t transitions = 0;
        const std::uint64_t end_us = (c.run_ms - 1) * 1000;
        for (const auto& [port, pin] : c.outputs) {
            auto o = ours.pins.count({port, pin}) ? ours.pins.at({port, pin}) : std::vector<Transition>{};
            auto t = theirs.pins.count({port, pin}) ? theirs.pins.at({port, pin}) : std::vector<Transition>{};
            if (o.size() == t.size() + 1 && o.back().time_us + tolerance(end_us) >= end_us) {
                o.pop_back();
            } else if (t.size() == o.size() + 1 && t.back().time_us + tolerance(end_us) >= end_us) {
                t.pop_back();
            }
            transitions += o.size();
            if (o.size() != t.size()) {
                std::printf("  %s P%c%d: %zu transitions in the emulator, %zu in Renode\n", c.firmware, 'A' + port, pin,
                    o.size(), t.size());
                ++problems;
                const std::size_t longest = std::max(o.size(), t.size());
                for (std::size_t i = longest > 3 ? longest - 3 : 0; i < longest; ++i) {
                    const std::string a = i < o.size() ? std::to_string(o[i].time_us) + (o[i].level ? " up" : " down") : "-";
                    const std::string b = i < t.size() ? std::to_string(t[i].time_us) + (t[i].level ? " up" : " down") : "-";
                    std::printf("    #%zu emulator %s  Renode %s\n", i, a.c_str(), b.c_str());
                }
            }
            for (std::size_t i = 0; i < std::min(o.size(), t.size()); ++i) {
                const std::uint64_t d = distance(o[i].time_us, t[i].time_us);
                worst = std::max(worst, d);
                if (o[i].level != t[i].level || d > tolerance(o[i].time_us)) {
                    std::printf("  %s P%c%d #%zu: emulator %d at %llu us, Renode %d at %llu us\n", c.firmware, 'A' + port,
                        pin, i, o[i].level ? 1 : 0, static_cast<unsigned long long>(o[i].time_us), t[i].level ? 1 : 0,
                        static_cast<unsigned long long>(t[i].time_us));
                    ++problems;
                }
            }
        }
        std::string text_o;
        std::string text_t;
        for (const auto& b : ours.uart) {
            text_o += static_cast<char>(b.second);
        }
        for (const auto& b : theirs.uart) {
            text_t += static_cast<char>(b.second);
        }
        if (text_o != text_t) {
            std::printf("  %s USART1 differs:\n    emulator \"%s\"\n    Renode   \"%s\"\n", c.firmware, text_o.c_str(),
                text_t.c_str());
            ++problems;
        } else {
            for (std::size_t i = 0; i < ours.uart.size(); ++i) {
                const std::uint64_t d = distance(ours.uart[i].first, theirs.uart[i].first);
                worst = std::max(worst, d);
                if (d > tolerance(ours.uart[i].first)) {
                    std::printf("  %s USART1 byte %zu at %llu us in the emulator, %llu us in Renode\n", c.firmware, i,
                        static_cast<unsigned long long>(ours.uart[i].first),
                        static_cast<unsigned long long>(theirs.uart[i].first));
                    ++problems;
                }
            }
        }
        std::printf("%s %s: %zu pin transitions, %zu UART bytes, largest time difference %llu us\n",
            problems ? "FAIL" : "PASS", c.firmware, transitions, ours.uart.size(), static_cast<unsigned long long>(worst));
        failures += problems ? 1 : 0;
    }
    return failures == 0 ? 0 : 1;
}
