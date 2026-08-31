// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
extern "C" {
#include "renode_api.h"
}
#include "mailbox.h"
#include "firmware_layout.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
int consume(renode_error_t* error)
{
    if (!error) { return ERR_NO_ERROR; }
    const int code = error->code;
    std::cerr << "Backend diagnostic " << code << ": " << (error->message ? error->message : "(none)") << '\n';
    renode_free_error(error);
    return code;
}
void success(renode_error_t* error) { require(consume(error) == ERR_NO_ERROR, "Backend operation failed"); }
template<class T> using Handle = std::unique_ptr<T, decltype(&std::free)>;
struct Session {
    renode_t* value = nullptr;
    ~Session() { if (value) { consume(renode_disconnect(&value)); } }
};
struct Event { uint64_t stamp, begin, end; int pin; bool state, during_run; };
struct Capture {
    std::array<Event, 512> events{};
    size_t count = 0;
    uint64_t begin = 0, end = 0;
    bool running = false, overflow = false;
};
struct PinCapture { Capture* capture; int pin; };
void changed(void* opaque, renode_gpio_event_data_t* data) noexcept
{
    auto& pin = *static_cast<PinCapture*>(opaque);
    auto& capture = *pin.capture;
    if (capture.count == capture.events.size()) { capture.overflow = true; return; }
    capture.events[capture.count++] = {data->timestamp_us, capture.begin, capture.end,
                                     pin.pin, data->state, capture.running};
}
struct Probe {
    Session session;
    Handle<renode_machine_t> machine{nullptr, &std::free};
    Handle<renode_gpio_t> gpio{nullptr, &std::free};
    Handle<renode_bus_context_t> bus{nullptr, &std::free};
    Capture capture;
    std::array<PinCapture, 3> pins{{{&capture, 0}, {&capture, 2}, {&capture, 4}}};
    uint64_t step;

    Probe(const char* port, uint64_t step_us) : step(step_us)
    {
        success(renode_connect(port, &session.value));
        renode_machine_t* m = nullptr;
        success(renode_get_machine(session.value, "sn012", &m)); machine.reset(m);
        renode_gpio_t* g = nullptr;
        success(renode_get_gpio(machine.get(), "gpioPortA", &g)); gpio.reset(g);
        renode_bus_context_t* b = nullptr;
        success(renode_get_sysbus(machine.get(), &b)); bus.reset(b);
        for (auto& pin : pins) {
            success(renode_register_gpio_state_change_callback(gpio.get(), pin.pin, &pin, changed));
        }
    }
    uint64_t now()
    {
        uint64_t value = 0;
        success(renode_get_current_time(session.value, TU_MICROSECONDS, &value));
        return value;
    }
    void advance(uint64_t target)
    {
        while (now() < target) {
            capture.begin = now(); capture.end = std::min(target, capture.begin + step);
            capture.running = true;
            auto* error = renode_run_for(session.value, TU_MICROSECONDS, capture.end - capture.begin);
            capture.running = false; success(error);
            require(now() == capture.end, "RunFor did not reach the exact requested time");
            require(!capture.overflow, "Event capture capacity exceeded");
        }
    }
    uint32_t read(uint64_t address)
    {
        uint32_t value = 0;
        success(renode_sysbus_read(bus.get(), address, AW_DOUBLE_WORD, &value, 1)); return value;
    }
    void write(uint64_t address, uint32_t value)
    {
        success(renode_sysbus_write(bus.get(), address, AW_DOUBLE_WORD, &value, 1));
    }
    sn_mailbox status()
    {
        sn_mailbox result{};
        success(renode_sysbus_read(bus.get(), SN_MAILBOX_ADDRESS, AW_MULTI_BYTE, &result, sizeof(result)));
        require(result.fault == 0, "Firmware entered its fault handler");
        return result;
    }
    bool state(int pin)
    {
        bool value = false; success(renode_get_gpio_state(gpio.get(), pin, &value)); return value;
    }
    void input(int pin, bool value)
    {
        const auto before = now();
        success(renode_set_gpio_state(gpio.get(), pin, value));
        require(now() == before, "Input command unexpectedly advanced time");
    }
    void snapshot(const char* name)
    {
        const auto data = status();
        std::cout << "{\"name\":\"" << name << "\",\"time_us\":" << now()
                  << ",\"ticks\":" << data.ticks << ",\"input\":" << data.input
                  << ",\"exti_count\":" << data.exti_count << ",\"exti_input\":" << data.exti_input
                  << ",\"pa2\":" << state(2) << ",\"pa4\":" << state(4) << '}';
    }
    void execute()
    {
        require(now() == 0, "Machine was not initially paused at zero");
        require(read(0) == 0x20005000 && read(0) == read(0x08000000)
                && read(4) == read(0x08000004), "Flash alias/vector map mismatch");
        // Poison RAM before boot: only executed startup may restore these results.
        write(SN_DATA_ADDRESS, 0xaaaaaaaa); write(SN_BSS_ADDRESS, 0xbbbbbbbb);
        advance(500);
        const auto boot = status();
        require(boot.magic == SN_BOOT_MAGIC && boot.data_check == SN_DATA_COOKIE && boot.bss_check == 0,
                "Firmware startup/data/BSS verification failed");
        require(boot.systick_reload == 7999 && (boot.systick_control & 7) == 7, "Wrong SysTick configuration");
        renode_adc_t* adc = nullptr;
        require(consume(renode_get_adc(machine.get(), "adc1", &adc)) == ERR_COMMAND_FAILED && !adc,
                "Offline profile unexpectedly exposes ADC");
        bool invalid_state = false;
        require(consume(renode_get_gpio_state(gpio.get(), 16, &invalid_state)) == ERR_COMMAND_FAILED,
                "Out-of-range GPIO output was not rejected");
        std::cout << "{\"step_us\":" << step << ",\"boot\":{\"magic\":" << boot.magic
                  << ",\"data\":" << boot.data_check << ",\"bss\":" << boot.bss_check
                  << ",\"adc_absent\":true,\"invalid_gpio_rejected\":true},\"snapshots\":[";
        snapshot("boot"); advance(2500); std::cout << ','; snapshot("before_rise");
        input(1, true); advance(2600);
        require(status().exti_count == 1 && status().exti_input == 1 && state(4), "Rising EXTI was not handled");
        std::cout << ','; snapshot("after_rise"); advance(4500);
        require(status().input == 1 && state(2), "Firmware did not sample high input");
        std::cout << ','; snapshot("sampled_high"); advance(5500); input(1, false); advance(5600);
        require(status().exti_count == 2 && status().exti_input == 0 && !state(4), "Falling EXTI was not handled");
        std::cout << ','; snapshot("after_fall"); advance(7500);
        require(status().input == 0 && !state(2), "Firmware did not sample low input");
        std::cout << ','; snapshot("sampled_low");
        // Repeated level must not create an extra edge-triggered interrupt.
        input(1, false); advance(7600);
        require(status().exti_count == 2, "Repeated input level created an interrupt");
        // Characterize pulses separately from the two required persistent inputs.
        input(1, true); advance(7620); input(1, false); advance(7800);
        std::cout << ','; snapshot("after_20us_pulse");
        require(status().exti_count == 4 && status().input == 0 && !state(2),
                "20 us pulse did not reach EXTI independently of periodic sampling");
        input(1, true); input(1, false); advance(7900);
        require(status().exti_count == 5 && status().exti_input == 0,
                "Same-time input edges changed their recorded collapse behavior");
        std::cout << ','; snapshot("after_same_time_edges");
        const size_t timing_events = capture.count;
        std::cout << "],\"modes\":[";
        struct Mode { const char* name; uint32_t bits, drive; };
        const std::array<Mode, 7> modes{{{"floating", 4, 0}, {"pull_down", 8, 0},
            {"pull_up", 8, 1}, {"analog", 0, 0}, {"push_pull", 2, 1},
            {"open_drain", 6, 1}, {"alternate_push_pull", 10, 0}}};
        uint32_t command = 0;
        for (const auto& mode : modes) {
            write(SN_MAILBOX_ADDRESS + offsetof(sn_mailbox, mode), mode.bits);
            write(SN_MAILBOX_ADDRESS + offsetof(sn_mailbox, drive), mode.drive);
            write(SN_MAILBOX_ADDRESS + offsetof(sn_mailbox, command), ++command);
            advance(now() + 2000);
            require(status().acknowledged == command && ((status().mode_readback >> 12) & 15) == mode.bits,
                    "Firmware mode command did not take effect");
            const bool before = state(3);
            input(3, false); const bool low = (read(0x40010808) & 8) != 0;
            input(3, true); const bool high = (read(0x40010808) & 8) != 0;
            if (command > 1) { std::cout << ','; }
            std::cout << "{\"name\":\"" << mode.name << "\",\"cnf_mode\":" << mode.bits
                      << ",\"drive\":" << mode.drive << ",\"before\":" << before
                      << ",\"injected_low\":" << low << ",\"injected_high\":" << high << '}';
        }
        const auto ticks_before = status().ticks;
        write(0x40021018, 0); advance(now() + 2000);
        const auto ticks_after = status().ticks;
        std::cout << "],\"rcc_audit\":{\"stored_enable\":" << read(0x40021018)
                  << ",\"ticks_before\":" << ticks_before << ",\"ticks_after\":" << ticks_after
                  << "},\"timing_event_count\":" << timing_events << ",\"events\":[";
        for (size_t i = 0; i < capture.count; ++i) {
            const auto& event = capture.events[i];
            if (i) { std::cout << ','; }
            std::cout << "{\"pin\":" << event.pin << ",\"state\":" << event.state
                      << ",\"timestamp_us\":" << event.stamp << ",\"request_begin_us\":" << event.begin
                      << ",\"request_end_us\":" << event.end << ",\"during_run\":" << event.during_run << '}';
        }
        std::cout << "],\"final_time_us\":" << now() << "}\n";
    }
};
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Usage: stm32_probe PORT STEP_US");
        const std::string step = argv[2];
        require(step == "100" || step == "1000", "Step must be 100 or 1000 us");
        Probe probe(argv[1], step == "100" ? 100 : 1000); probe.execute(); return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
