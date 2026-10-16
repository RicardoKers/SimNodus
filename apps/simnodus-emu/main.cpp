// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// simnodus-emu: runs STM32F103 firmware in the SimNodus emulator without the
// circuit (experiment E-07). Prints one line per pin change, UART byte and
// diagnostic, then a summary with the speed reached.
//
//   simnodus-emu firmware.elf [--ms 1000] [--info] [--quiet] [--count-ops]
//                [--input PA1=0@100] [--uart 1:text@50] [--gdb PORT]
//
// --input sets a pin level from outside at a time in milliseconds; --uart
// sends text to USART1..3 at a time in milliseconds; --gdb waits for a GDB
// client on 127.0.0.1:PORT (0: any free port) and runs under its control.
#include "emu/gdb_server.hpp"
#include "emu/stm32f103.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace simnodus::emu;

namespace {

struct Stimulus {
    std::uint64_t time_ns = 0;
    int port = -1; // pin input when >= 0
    int pin = 0;
    bool level = false;
    int usart = 0; // UART input when > 0
    std::string text;
};

bool parse_pin(const std::string& name, int& port, int& pin)
{
    if (name.size() < 3 || name[0] != 'P' || name[1] < 'A' || name[1] > 'E') {
        return false;
    }
    port = name[1] - 'A';
    pin = std::atoi(name.c_str() + 2);
    return pin >= 0 && pin < 16;
}

std::uint64_t parse_ms(const std::string& text)
{
    return static_cast<std::uint64_t>(std::strtod(text.c_str(), nullptr) * 1e6);
}

void usage()
{
    std::fprintf(stderr,
        "usage: simnodus-emu firmware.elf [--ms 1000] [--info] [--quiet] [--count-ops]\n"
        "                    [--input PA1=0@100] [--uart 1:text@50] [--gdb PORT]\n");
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage();
        return 2;
    }
    std::string path;
    double ms = 1000;
    bool info = false;
    bool quiet = false;
    bool count_ops = false;
    bool ms_given = false;
    int gdb_port = -1;
    std::vector<Stimulus> stimuli;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--ms" && has_value) {
            ms = std::strtod(argv[++i], nullptr);
            ms_given = true;
        } else if (arg == "--gdb" && has_value) {
            gdb_port = std::atoi(argv[++i]);
        } else if (arg == "--info") {
            info = true;
        } else if (arg == "--quiet") {
            quiet = true;
        } else if (arg == "--count-ops") {
            count_ops = true;
        } else if (arg == "--input" && has_value) {
            const std::string value = argv[++i];
            const auto eq = value.find('=');
            const auto at = value.find('@');
            Stimulus s;
            if (eq == std::string::npos || at == std::string::npos || !parse_pin(value.substr(0, eq), s.port, s.pin)) {
                usage();
                return 2;
            }
            s.level = value[eq + 1] == '1';
            s.time_ns = parse_ms(value.substr(at + 1));
            stimuli.push_back(s);
        } else if (arg == "--uart" && has_value) {
            const std::string value = argv[++i];
            const auto colon = value.find(':');
            const auto at = value.rfind('@');
            Stimulus s;
            if (colon == std::string::npos || at == std::string::npos || at < colon) {
                usage();
                return 2;
            }
            s.usart = std::atoi(value.substr(0, colon).c_str());
            s.text = value.substr(colon + 1, at - colon - 1);
            s.time_ns = parse_ms(value.substr(at + 1));
            stimuli.push_back(s);
        } else if (path.empty() && arg[0] != '-') {
            path = arg;
        } else {
            usage();
            return 2;
        }
    }

    const auto load_begin = std::chrono::steady_clock::now();
    std::string error;
    const auto image = read_elf(path, error);
    if (!image) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    Stm32F103 machine;
    if (!machine.load(*image, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    const double load_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - load_begin).count();
    if (info) {
        std::printf("entry 0x%08X, %zu segment(s), %zu function symbol(s), loaded and reset in %.2f ms\n",
            static_cast<unsigned>(image->entry), image->segments.size(), image->functions.size(), load_ms);
        for (const auto& s : image->segments) {
            std::printf("  segment 0x%08X (virtual 0x%08X): %zu bytes in file, %u in memory%s\n",
                static_cast<unsigned>(s.physical), static_cast<unsigned>(s.virtual_address), s.data.size(),
                static_cast<unsigned>(s.memory_size), s.executable ? ", executable" : "");
        }
    }
    machine.cpu().count_ops(count_ops);

    std::sort(stimuli.begin(), stimuli.end(), [](const Stimulus& a, const Stimulus& b) { return a.time_ns < b.time_ns; });
    std::unique_ptr<GdbServer> gdb;
    if (gdb_port >= 0) {
        gdb = std::make_unique<GdbServer>(machine);
        if (!gdb->start(static_cast<std::uint16_t>(gdb_port), error)) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
        std::printf("GDB server on 127.0.0.1:%u; waiting for a client (target extended-remote :%u)\n",
            static_cast<unsigned>(gdb->port()), static_cast<unsigned>(gdb->port()));
        std::fflush(stdout);
        while (!gdb->attached()) {
            gdb->service(0);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!ms_given) {
            ms = 3.6e6; // an hour of firmware time
        }
    }
    const std::uint64_t end_ns = static_cast<std::uint64_t>(ms * 1e6);
    std::size_t next = 0;
    std::string uart_line[4];
    const auto begin = std::chrono::steady_clock::now();
    const std::uint64_t instructions_before = machine.cpu().instructions();
    auto flush = [&] {
        for (const auto& e : machine.take_pin_events()) {
            if (!quiet) {
                std::printf("%14.6f ms  P%c%d %d\n", static_cast<double>(e.time_ns) / 1e6, 'A' + e.port, e.pin,
                    e.level ? 1 : 0);
            }
        }
        for (const auto& b : machine.take_uart_bytes()) {
            if (!quiet) {
                const char c = static_cast<char>(b.value);
                if (c == '\n' || uart_line[b.usart].size() > 200) {
                    std::printf("%14.6f ms  USART%d \"%s\"\n", static_cast<double>(b.time_ns) / 1e6, b.usart,
                        uart_line[b.usart].c_str());
                    uart_line[b.usart].clear();
                } else if (c != '\r') {
                    uart_line[b.usart] += (c >= 32 && c < 127) ? c : '.';
                }
            }
        }
        for (const auto& d : machine.take_diagnostics()) {
            std::printf("%14.6f ms  diagnostic: %s\n", static_cast<double>(d.time_ns) / 1e6, d.text.c_str());
        }
    };
    while (machine.time_ns() < end_ns) {
        if (gdb && !gdb->service(50)) {
            continue; // halted by the debugger
        }
        std::uint64_t until = next < stimuli.size() ? std::min(stimuli[next].time_ns, end_ns) : end_ns;
        if (gdb) {
            until = std::min(until, machine.time_ns() + 1000000); // let the debugger in every millisecond
        }
        machine.run_until_ns(until);
        if (gdb) {
            gdb->after_run();
        }
        flush();
        while (next < stimuli.size() && stimuli[next].time_ns <= machine.time_ns()) {
            const Stimulus& s = stimuli[next++];
            if (s.port >= 0) {
                machine.set_pin_input(s.port, s.pin, s.level);
            } else {
                for (const char c : s.text) {
                    machine.uart_receive(s.usart, static_cast<std::uint8_t>(c));
                }
            }
        }
    }
    flush();
    for (int u = 1; u <= 3; ++u) {
        if (!uart_line[u].empty() && !quiet) {
            std::printf("%14s     USART%d \"%s\" (no newline yet)\n", "", u, uart_line[u].c_str());
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    const std::uint64_t executed = machine.cpu().instructions() - instructions_before;
    std::printf("simulated %.3f ms in %.3f s (%.2fx real time); %llu instructions, %.1f MIPS; HCLK %u Hz%s\n",
        static_cast<double>(machine.time_ns()) / 1e6, seconds, (static_cast<double>(machine.time_ns()) / 1e9) / seconds,
        static_cast<unsigned long long>(executed), static_cast<double>(executed) / seconds / 1e6, static_cast<unsigned>(machine.hclk_hz()),
        machine.cpu().state() == CoreState::Lockup ? ", core locked up" : "");
    if (info) {
        const ElfSymbol* function = image->function_at(machine.cpu().pc());
        std::printf("final PC 0x%08X%s%s\n", static_cast<unsigned>(machine.cpu().pc()), function ? " in " : "",
            function ? function->name.c_str() : "");
    }
    if (count_ops) {
        const auto& hits = machine.cpu().op_hits();
        for (int op = 1; op < static_cast<int>(hits.size()); ++op) {
            if (hits[static_cast<std::size_t>(op)]) {
                std::printf("  %-28s %llu\n", Cpu::op_name(static_cast<std::uint8_t>(op)),
                    static_cast<unsigned long long>(hits[static_cast<std::size_t>(op)]));
            }
        }
    }
    return machine.cpu().state() == CoreState::Lockup ? 3 : 0;
}
