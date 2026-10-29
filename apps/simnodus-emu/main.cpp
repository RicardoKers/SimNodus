// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// simnodus-emu: runs STM32F103 firmware in the SimNodus emulator without the
// circuit (experiment E-07). Prints one line per pin change, UART byte and
// diagnostic, then a summary with the speed reached. A RISC-V ELF runs on the
// bare RISC-V test machine instead (experiment E-08: the RV32IMC core with
// RAM, UART, timer and test finisher at QEMU virt's addresses).
//
//   simnodus-emu firmware.elf [--ms 1000] [--info] [--quiet] [--count-ops]
//                [--input PA1=0@100] [--uart 1:text@50] [--gdb PORT]
//
// --input sets a pin level from outside at a time in milliseconds; --uart
// sends text to USART1..3 at a time in milliseconds; --gdb waits for a GDB
// client on 127.0.0.1:PORT (0: any free port) and runs under its control.
#include "devices/host_link.hpp"
#include "emu/esp32c3.hpp"
#include "emu/gdb_server.hpp"
#include "emu/riscv_test_machine.hpp"
#include "emu/stm32f103.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
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

// The RV32IMC core on the test machine: UART lines, then the speed reached.
int run_riscv(const ElfImage& image, double ms, bool quiet, bool count_entries)
{
    RiscvTestMachine machine;
    std::string error;
    if (!machine.load(image, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    machine.cpu().count_entries(count_entries);
    const std::uint64_t end_ns = static_cast<std::uint64_t>(ms * 1e6);
    std::size_t printed = 0;
    std::string line;
    const auto begin = std::chrono::steady_clock::now();
    while (machine.time_ns() < end_ns && !machine.finished() && machine.cpu().state() != RvState::Halted) {
        machine.run_until_ns(std::min(end_ns, machine.time_ns() + 10'000'000)); // 10 ms slices for the output
        const std::string& output = machine.output();
        for (; printed < output.size(); ++printed) {
            const char c = output[printed];
            if (c == '\n' || line.size() > 200) {
                if (!quiet) {
                    std::printf("%14.6f ms  UART \"%s\"\n", static_cast<double>(machine.time_ns()) / 1e6, line.c_str());
                }
                line.clear();
            } else if (c != '\r') {
                line += (c >= 32 && c < 127) ? c : '.';
            }
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    const std::uint64_t executed = machine.cpu().instructions();
    std::printf("simulated %.3f ms in %.3f s (%.2fx real time at 160 MHz); %llu instructions, %.1f MIPS%s\n",
        static_cast<double>(machine.time_ns()) / 1e6, seconds, (static_cast<double>(machine.time_ns()) / 1e9) / seconds,
        static_cast<unsigned long long>(executed), static_cast<double>(executed) / seconds / 1e6,
        machine.finished() ? (machine.exit_code() == 0 ? "; finished (pass)" : "; finished (fail)") : "");
    if (machine.cpu().state() == RvState::Halted) {
        std::printf("core halted: %s\n", machine.cpu().halt_reason().c_str());
    }
    if (count_entries) {
        const auto& hits = machine.cpu().entry_hits();
        for (std::size_t e = 1; e < hits.size(); ++e) {
            if (hits[e]) {
                std::printf("  %-12s %llu\n", RiscvCpu::entry_name(static_cast<std::uint8_t>(e)),
                    static_cast<unsigned long long>(hits[e]));
            }
        }
    }
    if (machine.finished()) {
        return machine.exit_code() == 0 ? 0 : 4;
    }
    return machine.cpu().state() == RvState::Halted ? 3 : 0;
}

// ESP32-C3 (E-08, SN-244): a flash image (a merged .bin at 0, or file@offset),
// the ROM ELF, and ELF files whose symbols name functions in diagnostics.
int run_esp32c3(int argc, char** argv)
{
    std::string rom = "third_party/esp-rom-elfs/esp32c3_rev3_rom.elf";
    std::vector<std::pair<std::string, std::uint32_t>> images;
    std::vector<std::string> elfs;
    double ms = 1000;
    bool quiet = false;
    bool trace = false;
    struct Typed {
        std::uint64_t time_ns;
        int port;
        std::string text;
    };
    std::vector<Typed> typed; // --usb text@ms, --uart0 text@ms; \n and \r in the text
    std::string flash_out;
    Esp32C3::Options options;
    std::vector<std::pair<int, std::unique_ptr<simnodus::devices::HostLink>>> links; // --link: real time, no DTR/RTS
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--usb" || arg == "--uart0" || arg == "--uart1") && i + 1 < argc) {
            const std::string value = argv[++i];
            const auto at = value.rfind('@');
            std::string text = value.substr(0, at);
            for (const auto& [escape, byte] : {std::pair<std::string, char>{"\\n", '\n'}, {"\\r", '\r'}}) {
                for (auto pos = text.find(escape); pos != std::string::npos; pos = text.find(escape, pos + 1)) {
                    text.replace(pos, 2, 1, byte);
                }
            }
            // \xHH: any byte (esptool's SLIP frames).
            for (auto pos = text.find("\\x"); pos != std::string::npos && pos + 4 <= text.size(); pos = text.find("\\x", pos + 1)) {
                text.replace(pos, 4, 1, static_cast<char>(std::strtoul(text.substr(pos + 2, 2).c_str(), nullptr, 16)));
            }
            typed.push_back({at == std::string::npos ? 0 : static_cast<std::uint64_t>(std::strtod(value.c_str() + at + 1, nullptr) * 1e6),
                arg == "--usb" ? 2 : arg == "--uart1" ? 1 : 0, text});
        } else if (arg == "--input" && i + 1 < argc) { // GPIO=0|1@ms (a level) or GPIO=1.2V@ms (a voltage for the ADC)
            const std::string value = argv[++i];
            const auto eq = value.find('=');
            const auto at = value.rfind('@');
            if (eq == std::string::npos || at == std::string::npos) {
                std::fprintf(stderr, "error: --input takes GPIO=0|1@ms\n");
                return 2;
            }
            typed.push_back({static_cast<std::uint64_t>(std::strtod(value.c_str() + at + 1, nullptr) * 1e6),
                -1 - std::atoi(value.c_str()), value.substr(eq + 1, at - eq - 1)}); // 0, 1 or a voltage like 1.2V
        } else if (arg == "--link" && i + 1 < argc) { // PORT=LINK: a serial port of the computer or tcp:4000
            const std::string value = argv[++i];
            const auto eq = value.find('=');
            const std::string port = value.substr(0, eq);
            std::string why;
            auto link = simnodus::devices::open_host_link(value.substr(eq + 1), why);
            if (eq == std::string::npos || !link) {
                std::fprintf(stderr, "error: --link takes uart0|uart1|usb=COM7|tcp:4000 (%s)\n", why.c_str());
                return 2;
            }
            links.emplace_back(port == "usb" ? 2 : port == "uart1" ? 1 : 0, std::move(link));
        } else if (arg == "--strap" && i + 1 < argc) {
            options.strapping = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 0)); // 0x4: download mode (GPIO9 low)
        } else if (arg == "--flash-out" && i + 1 < argc) {
            flash_out = argv[++i]; // the whole flash at the end (pass it back as the image to continue)
        } else if (arg == "--rom" && i + 1 < argc) {
            rom = argv[++i];
        } else if (arg == "--elf" && i + 1 < argc) {
            elfs.push_back(argv[++i]);
        } else if (arg == "--ms" && i + 1 < argc) {
            ms = std::strtod(argv[++i], nullptr);
        } else if (arg == "--quiet") {
            quiet = true;
        } else if (arg == "--trace-io") {
            trace = true;
        } else if (arg[0] != '-') {
            const auto at = arg.rfind('@');
            const std::uint32_t offset = at == std::string::npos ? 0 : static_cast<std::uint32_t>(std::strtoul(arg.c_str() + at + 1, nullptr, 0));
            images.emplace_back(arg.substr(0, at), offset);
        } else {
            std::fprintf(stderr, "usage: simnodus-emu --esp32c3 image.bin[@offset]... [--elf app.elf] [--rom rom.elf] [--ms 1000] [--quiet] [--trace-io]\n");
            return 2;
        }
    }
    std::string error;
    const auto rom_elf = read_elf(rom, error, ElfMachine::Riscv);
    Esp32C3 machine(options);
    if (!rom_elf || !machine.load_rom(*rom_elf, error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    for (const auto& [path, offset] : images) {
        std::ifstream stream(path, std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        if (!stream && bytes.empty()) {
            std::fprintf(stderr, "error: cannot read %s\n", path.c_str());
            return 1;
        }
        if (!machine.load_flash(offset, bytes, error)) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
    }
    for (const std::string& path : elfs) {
        if (const auto elf = read_elf(path, error, ElfMachine::Riscv)) {
            machine.add_symbols(*elf);
        } else {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
    }
    if (trace) {
        machine.set_trace([&machine](const std::string& text) {
            std::printf("%14.6f ms  %s\n", static_cast<double>(machine.time_ns()) / 1e6, text.c_str());
        });
    }
    machine.reset();
    static const char* const ports[] = {"UART0", "UART1", "USB"};
    std::string lines[3];
    const std::uint64_t end_ns = static_cast<std::uint64_t>(ms * 1e6);
    const auto begin = std::chrono::steady_clock::now();
    std::sort(typed.begin(), typed.end(), [](const Typed& a, const Typed& b) { return a.time_ns < b.time_ns; });
    std::size_t next_typed = 0;
    std::vector<bool> pins_shown(Esp32C3::gpio_count, false); // print pads once they are outputs
    while (machine.time_ns() < end_ns && machine.cpu().state() != RvState::Halted) {
        std::uint64_t until = std::min(end_ns, machine.time_ns() + 1'000'000);
        if (next_typed < typed.size()) {
            until = std::max(machine.time_ns() + 1, std::min(until, typed[next_typed].time_ns));
        }
        if (!links.empty()) {
            // Linked to a program: at most real-time speed, its bytes in.
            until = std::min(until, machine.time_ns() + 200'000);
            const auto real = begin + std::chrono::nanoseconds(machine.time_ns());
            std::this_thread::sleep_until(real);
            for (auto& [port, link] : links) {
                std::uint8_t buffer[1024];
                for (std::size_t n; (n = link->read(buffer, sizeof(buffer))) > 0;) {
                    machine.send(port, std::string(reinterpret_cast<const char*>(buffer), n));
                }
            }
        }
        machine.run_until_ns(until);
        while (next_typed < typed.size() && typed[next_typed].time_ns <= machine.time_ns()) {
            const Typed& t = typed[next_typed++];
            if (t.port < 0) { // --input: port -1 - GPIO
                if (!t.text.empty() && (t.text.back() == 'V' || t.text.back() == 'v')) {
                    machine.set_pin_voltage(-1 - t.port, std::strtod(t.text.c_str(), nullptr));
                } else {
                    machine.set_pin_input(-1 - t.port, t.text == "1");
                }
            } else {
                machine.send(t.port, t.text);
            }
        }
        auto output = machine.take_output();
        for (auto& [port, link] : links) {
            std::string bytes;
            for (const auto& out : output) {
                if (out.port == port) {
                    bytes += static_cast<char>(out.value);
                }
            }
            link->write(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
        }
        for (const auto& out : output) {
            std::string& line = lines[out.port];
            if (out.value == '\n' || line.size() > 200) {
                if (!quiet) {
                    std::printf("%14.6f ms  %-5s \"%s\"\n", static_cast<double>(out.time_ns) / 1e6, ports[out.port], line.c_str());
                }
                line.clear();
            } else if (out.value != '\r') {
                line += (out.value >= 32 && out.value < 127) ? static_cast<char>(out.value) : '.';
            }
        }
        for (const auto& text : machine.take_diagnostics()) {
            std::printf("%14.6f ms  diagnostic: %s\n", static_cast<double>(machine.time_ns()) / 1e6, text.c_str());
        }
        for (const auto& e : machine.take_pin_events()) {
            if (!quiet && (e.state.output || pins_shown[static_cast<std::size_t>(e.pin)])) {
                std::printf("%14.6f ms  GPIO%d %s\n", static_cast<double>(e.time_ns) / 1e6, e.pin,
                    e.state.output ? (e.state.level ? "1" : "0") : e.state.peripheral ? "peripheral" : "input");
                pins_shown[static_cast<std::size_t>(e.pin)] = e.state.output;
            }
        }
    }
    for (int p = 0; p < 3; ++p) {
        if (!lines[p].empty() && !quiet) {
            std::printf("%14s     %-5s \"%s\" (no newline yet)\n", "", ports[p], lines[p].c_str());
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    if (!flash_out.empty()) {
        std::ofstream out(flash_out, std::ios::binary);
        out.write(reinterpret_cast<const char*>(machine.flash().data()), static_cast<std::streamsize>(machine.flash().size()));
        if (!out) {
            std::fprintf(stderr, "error: cannot write %s\n", flash_out.c_str());
        }
    }
    const std::string where = machine.function_at(machine.cpu().pc());
    std::printf("simulated %.3f ms in %.3f s; %llu instructions, %.1f MIPS; CPU %u Hz; PC 0x%08X%s%s\n",
        static_cast<double>(machine.time_ns()) / 1e6, seconds, static_cast<unsigned long long>(machine.cpu().instructions()),
        static_cast<double>(machine.cpu().instructions()) / seconds / 1e6, static_cast<unsigned>(machine.cpu_hz()),
        static_cast<unsigned>(machine.cpu().pc()), where.empty() ? "" : " in ", where.c_str());
    return machine.cpu().state() == RvState::Halted ? 3 : 0;
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
    if (std::strcmp(argv[1], "--esp32c3") == 0) {
        return run_esp32c3(argc, argv);
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
        std::string riscv_error;
        if (const auto riscv = read_elf(path, riscv_error, ElfMachine::Riscv)) {
            return run_riscv(*riscv, ms, quiet, count_ops);
        }
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
