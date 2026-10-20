// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "native_mcu_runner.hpp"

#include "device_bus.hpp"

#include "emu/elf.hpp"
#include "emu/gdb_server.hpp"
#include "emu/stm32f103.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>

namespace simnodus::mcu {

namespace {

constexpr std::uint64_t history_keep_ns = 100000000; // 100 ms
constexpr std::size_t uart_keep = 1 << 20;
constexpr std::size_t log_keep = 50000;

// Bounds a pin history that the circuit never reads (a pin without a wire):
// drives_at() prunes the others. Keeps the window the circuit may still ask for.
template <typename History, typename Time>
void trim_history(History& history, Time keep)
{
    if (history.size() < 1024) {
        return;
    }
    const Time newest = history.back().first;
    const Time horizon = newest > keep ? newest - keep : 0;
    auto first_kept = std::upper_bound(history.begin(), history.end(), horizon,
        [](Time t, const auto& entry) { return t < entry.first; });
    if (first_kept != history.begin()) {
        --first_kept; // the state in force at the horizon
    }
    history.erase(history.begin(), first_kept);
}

// "PA9" -> {0, 9}; {-1, -1} for anything else.
std::pair<int, int> port_pin(const std::string& name)
{
    if (name.size() < 3 || name[0] != 'P' || name[1] < 'A' || name[1] > 'E') {
        return {-1, -1};
    }
    const int pin = std::atoi(name.c_str() + 2);
    return pin >= 0 && pin < 16 ? std::pair{name[1] - 'A', pin} : std::pair{-1, -1};
}

// `level`: the level on the pin, shown for inputs (outputs show what they drive).
PinState pin_state(std::uint8_t mode, std::uint8_t cnf, bool drive_high, bool pull_up, bool level)
{
    PinState s;
    if (mode == 0) {
        switch (cnf) {
        case 0: s.mode = PinMode::Analog; break;
        case 2: s.mode = PinMode::Input; s.pull = pull_up ? Pull::Up : Pull::Down; break;
        default: s.mode = PinMode::Input; break;
        }
        s.level = level;
        return s;
    }
    s.mode = cnf >= 2 ? PinMode::Alternate : PinMode::Output;
    s.open_drain = (cnf & 1u) != 0;
    s.level = drive_high;
    return s;
}

} // namespace

NativeMcuRunner::NativeMcuRunner(McuRunOptions options) : options_(std::move(options))
{
    // Device pins (SN-203) follow the GPIO pins, named "U2.Q0".
    pin_models_.assign(options_.pins.size(), options_.pin_model);
    for (const auto& device : options_.devices) {
        auto& indexes = device_pins_.emplace_back();
        for (const auto& pin : device.pins) {
            device_of_pin_[options_.pins.size()] = {device_pins_.size() - 1, indexes.size()};
            indexes.push_back(options_.pins.size());
            options_.pins.push_back(device.designator + "." + pin);
            pin_models_.push_back(device.pin_model);
        }
    }
    history_.resize(options_.pins.size());
    levels_.assign(options_.pins.size(), {{0, false}});
    input_desired_.assign(options_.pins.size(), false);
    analog_.resize(options_.pins.size());
    for (std::size_t i = 0; i < options_.pins.size(); ++i) {
        ports_.push_back(port_pin(options_.pins[i]));
        history_[i].push_back({0, PinState{}});
    }
    for (const auto& uart : options_.uarts) {
        UartLog log;
        log.number = uart.empty() ? 1 : uart.back() - '0';
        uarts_[uart] = log;
    }
}

NativeMcuRunner::~NativeMcuRunner()
{
    stop();
}

void NativeMcuRunner::start()
{
    {
        std::lock_guard lock(mutex_);
        status_ = McuStatus{};
        status_.state = McuState::Starting;
    }
    stop_ = false;
    worker_ = std::thread([this] { work(); });
}

bool NativeMcuRunner::wait_ready(const std::atomic<bool>& cancel)
{
    std::unique_lock lock(mutex_);
    while (status_.state == McuState::Starting && !cancel) {
        changed_.wait_for(lock, std::chrono::milliseconds(5));
    }
    return status_.state == McuState::Running;
}

void NativeMcuRunner::set_limit_ns(std::uint64_t limit)
{
    {
        std::lock_guard lock(mutex_);
        limit_ns_ = std::max(limit_ns_, limit);
    }
    changed_.notify_all();
}

bool NativeMcuRunner::wait_time_ns(std::uint64_t time_ns, const std::atomic<bool>& cancel)
{
    std::unique_lock lock(mutex_);
    while (status_.time_ns < time_ns && status_.state == McuState::Running && !cancel) {
        changed_.wait_for(lock, std::chrono::milliseconds(5));
    }
    return status_.time_ns >= time_ns;
}

void NativeMcuRunner::request_reset()
{
    reset_ = true;
    changed_.notify_all();
}

void NativeMcuRunner::set_input(std::size_t pin, bool level, std::uint64_t time_ns, std::uint64_t changed_ns)
{
    {
        std::lock_guard lock(mutex_);
        if (pin >= input_desired_.size() || input_desired_[pin] == level) {
            return;
        }
        input_desired_[pin] = level;
        input_queue_.push_back({pin, level, time_ns, std::min(changed_ns, time_ns)});
    }
    changed_.notify_all();
}

void NativeMcuRunner::set_analog(std::uint64_t time_ns, const std::vector<double>& volts)
{
    std::lock_guard lock(mutex_);
    for (std::size_t i = 0; i < volts.size() && i < analog_.size(); ++i) {
        if (std::isnan(volts[i])) {
            continue;
        }
        auto& history = analog_[i];
        if (!history.empty() && history.back().first >= time_ns) {
            history.back() = {time_ns, volts[i]}; // the same point again
            continue;
        }
        history.emplace_back(time_ns, volts[i]);
        if (history.size() > 4096) {
            history.erase(history.begin(), history.begin() + 2048); // far older than any sample needs
        }
    }
}

double NativeMcuRunner::analog_value(std::size_t pin, std::uint64_t time_ns)
{
    // The voltage at the end of the sample's window minus the lag: a time the
    // circuit has passed whenever the firmware may run this window, so the code
    // does not depend on thread timing (MCU_EMULATOR.md section 7).
    const std::uint64_t window_ns = std::max<std::uint64_t>(options_.chunk_us, 1) * 1000;
    const std::uint64_t window_end = (time_ns / window_ns + 1) * window_ns;
    const std::uint64_t at = window_end > options_.analog_lag_ns ? window_end - options_.analog_lag_ns : 0;
    std::lock_guard lock(mutex_);
    if (pin >= analog_.size() || analog_[pin].empty()) {
        return 0.0; // no node: an open pin reads 0 V
    }
    const auto& history = analog_[pin];
    auto after = std::lower_bound(history.begin(), history.end(), at,
        [](const auto& entry, std::uint64_t t) { return entry.first < t; });
    if (after == history.end()) {
        ++status_.late_analog_samples; // the circuit is not there yet: its latest value
        return history.back().second;
    }
    if (after == history.begin() || after->first == at) {
        return after->second;
    }
    const auto before = std::prev(after);
    const double f = static_cast<double>(at - before->first) / static_cast<double>(after->first - before->first);
    return before->second + f * (after->second - before->second);
}

void NativeMcuRunner::uart_send(const std::string& uart, const std::string& bytes)
{
    {
        std::lock_guard lock(mutex_);
        if (const auto it = uarts_.find(uart); it != uarts_.end()) {
            it->second.pending += bytes;
        }
    }
    changed_.notify_all();
}

std::uint64_t NativeMcuRunner::uart_received(
    const std::string& uart, std::uint64_t from, std::string& bytes, std::vector<std::uint64_t>* times) const
{
    std::lock_guard lock(mutex_);
    bytes.clear();
    if (times) {
        times->clear();
    }
    const auto it = uarts_.find(uart);
    if (it == uarts_.end()) {
        return 0;
    }
    const auto& log = it->second;
    const std::uint64_t first = log.total - log.bytes.size();
    const std::uint64_t start = std::max(from, first);
    if (start < log.total) {
        const auto offset = static_cast<std::size_t>(start - first);
        bytes = log.bytes.substr(offset);
        if (times) {
            times->assign(log.times.begin() + static_cast<std::ptrdiff_t>(offset), log.times.end());
        }
    }
    return log.total;
}

void NativeMcuRunner::stop()
{
    stop_ = true;
    changed_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard lock(mutex_);
    if (status_.state == McuState::Running || status_.state == McuState::Starting) {
        status_.state = McuState::Stopped;
    }
}

McuStatus NativeMcuRunner::status() const
{
    std::lock_guard lock(mutex_);
    return status_;
}

std::map<std::string, PinState> NativeMcuRunner::pins() const
{
    std::lock_guard lock(mutex_);
    std::map<std::string, PinState> result;
    for (std::size_t i = 0; i < options_.pins.size(); ++i) {
        result[options_.pins[i]] = history_[i].back().second;
    }
    return result;
}

std::vector<PinDrive> NativeMcuRunner::drives_at(std::uint64_t time_ns)
{
    std::lock_guard lock(mutex_);
    std::vector<PinDrive> drives(options_.pins.size());
    for (std::size_t i = 0; i < history_.size(); ++i) {
        auto& history = history_[i];
        auto it = std::upper_bound(history.begin(), history.end(), time_ns,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        const auto current = it == history.begin() ? history.begin() : std::prev(it);
        drives[i] = pin_drive(current->second, pin_models_[i]);
        const std::uint64_t horizon = time_ns > history_keep_ns ? time_ns - history_keep_ns : 0;
        auto keep = std::upper_bound(history.begin(), history.end(), horizon,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        if (keep != history.begin() && std::prev(keep) != history.begin()) {
            history.erase(history.begin(), std::prev(keep));
        }
    }
    return drives;
}

std::vector<PinDrive> NativeMcuRunner::average_drives(std::uint64_t from_ns, std::uint64_t to_ns)
{
    std::vector<PinDrive> drives = drives_at(to_ns); // also prunes old history
    if (to_ns <= from_ns) {
        return drives;
    }
    std::lock_guard lock(mutex_);
    const double span = static_cast<double>(to_ns - from_ns);
    for (std::size_t i = 0; i < history_.size(); ++i) {
        const auto& history = history_[i];
        auto it = std::upper_bound(history.begin(), history.end(), from_ns,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        if (it == history.end() || it->first >= to_ns) {
            continue; // no change inside the step: the drive at its end
        }
        // Time-weighted drive: the state in force at from_ns, then each change.
        auto current = it == history.begin() ? it : std::prev(it);
        PinDrive sum;
        std::uint64_t at = from_ns;
        for (;;) {
            const auto next_entry = std::next(current);
            const std::uint64_t until =
                next_entry != history.end() && next_entry->first < to_ns ? next_entry->first : to_ns;
            const PinDrive d = pin_drive(current->second, pin_models_[i]);
            const double weight = static_cast<double>(until - at);
            sum.conductance += d.conductance * weight;
            sum.current += d.current * weight;
            if (until >= to_ns) {
                break;
            }
            at = until;
            current = next_entry;
        }
        drives[i].conductance = sum.conductance / span;
        drives[i].current = sum.current / span;
    }
    return drives;
}

std::vector<std::uint64_t> NativeMcuRunner::drive_changes(
    const std::vector<std::size_t>& pins, std::uint64_t from_ns, std::uint64_t to_ns) const
{
    std::lock_guard lock(mutex_);
    std::vector<std::uint64_t> times;
    for (const std::size_t pin : pins) {
        if (pin >= history_.size()) {
            continue;
        }
        const auto& history = history_[pin];
        auto it = std::upper_bound(history.begin(), history.end(), from_ns,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        for (; it != history.end() && it->first <= to_ns; ++it) {
            if (it == history.begin()) {
                continue;
            }
            const PinDrive now = pin_drive(it->second, pin_models_[pin]);
            const PinDrive before = pin_drive(std::prev(it)->second, pin_models_[pin]);
            if (now.conductance != before.conductance || now.current != before.current) {
                times.push_back(it->first);
            }
        }
    }
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end()), times.end());
    return times;
}

PinLog NativeMcuRunner::pin_log(std::size_t pin, std::uint64_t from_ns) const
{
    std::lock_guard lock(mutex_);
    PinLog result;
    if (pin >= levels_.size()) {
        return result;
    }
    const auto& log = levels_[pin];
    auto it = std::upper_bound(log.begin(), log.end(), from_ns,
        [](std::uint64_t t, const auto& entry) { return t < entry.first; });
    result.initial = it == log.begin() ? log.front().second : std::prev(it)->second;
    result.changes.assign(it, log.end());
    return result;
}

void NativeMcuRunner::set_device_parameter(std::uint64_t component, const std::string& name, const std::string& value)
{
    {
        std::lock_guard lock(mutex_);
        parameter_changes_.push_back({component, name, value});
    }
    changed_.notify_all();
}

bool NativeMcuRunner::device_memory(std::uint64_t component, std::vector<std::uint8_t>& bytes) const
{
    std::lock_guard lock(mutex_);
    const auto it = memories_.find(component);
    if (it == memories_.end()) {
        return false;
    }
    bytes = it->second.bytes;
    return true;
}

void NativeMcuRunner::load_device_memory(std::uint64_t component, const std::vector<std::uint8_t>& bytes)
{
    {
        std::lock_guard lock(mutex_);
        memory_loads_.emplace_back(component, bytes);
    }
    changed_.notify_all();
}

std::map<std::uint64_t, std::string> NativeMcuRunner::device_texts() const
{
    std::lock_guard lock(mutex_);
    return device_texts_;
}

std::vector<std::string> NativeMcuRunner::take_diagnostics()
{
    std::lock_guard lock(mutex_);
    return std::exchange(diagnostics_, {});
}

void NativeMcuRunner::fail(const std::string& message)
{
    {
        std::lock_guard lock(mutex_);
        status_.state = McuState::Failed;
        status_.message = message;
    }
    changed_.notify_all();
}

void NativeMcuRunner::sample_pins(emu::Stm32F103& machine, std::uint64_t time_ns)
{
    for (std::size_t i = 0; i < ports_.size(); ++i) {
        if (ports_[i].first < 0) {
            continue;
        }
        const auto c = machine.pin_config(ports_[i].first, ports_[i].second);
        const PinState state = pin_state(c.mode, c.cnf, c.drive_high, c.pull_up, c.output_level);
        const PinState& last = history_[i].back().second;
        if (!state.same_config(last) || state.level != last.level) {
            history_[i].push_back({std::max(time_ns, history_[i].back().first), state});
            trim_history(history_[i], history_keep_ns);
        }
    }
}

void NativeMcuRunner::work()
{
    const auto begin = std::chrono::steady_clock::now();
    std::string error;
    emu::MachineOptions machine_options;
    // A frame on the RX pin is decided once every edge up to its stop bit has
    // arrived: the ADC's lag plus two windows (an input is applied at a window
    // boundary, while the firmware may be a window further on).
    machine_options.uart_rx_lag_ns = options_.analog_lag_ns + 2 * std::max<std::uint64_t>(options_.chunk_us, 1) * 1000;
    machine_options.uart_rx_min_bit_ns = options_.uart_rx_min_bit_ns;
    for (const auto& uart : options_.uart_rx_byte_links) {
        if (!uart.empty() && uart.back() >= '1' && uart.back() <= '3') {
            machine_options.uart_rx_pins &= ~(1u << (uart.back() - '1'));
        }
    }
    auto machine = std::make_unique<emu::Stm32F103>(machine_options);
    // Without a firmware file (debugging only) the flash stays empty until the
    // debugger's `load` writes it.
    if (options_.chips_only) {
        // Chips without a board (SN-212): a core that sleeps (WFI) until the devices' next event.
        emu::ElfImage image;
        std::vector<std::uint8_t> bytes(0x24, 0);
        const std::uint32_t vectors[2] = {0x20005000u, 0x08000021u}; // stack, reset handler
        std::memcpy(bytes.data(), vectors, sizeof vectors);
        bytes[0x20] = 0x30; // wfi
        bytes[0x21] = 0xBF;
        bytes[0x22] = 0xFD; // b wfi
        bytes[0x23] = 0xE7;
        image.entry = 0x08000021;
        image.segments.push_back({0x08000000, 0x08000000, bytes, static_cast<std::uint32_t>(bytes.size()), true});
        if (!machine->load(image, error)) {
            fail(error);
            return;
        }
    } else if (!options_.firmware.empty() || options_.gdb_port < 0) {
        const auto image = emu::read_elf(options_.firmware, error);
        if (!image || !machine->load(*image, error)) {
            fail(error);
            return;
        }
    }
    std::map<std::pair<int, int>, std::size_t> watched;
    for (std::size_t i = 0; i < ports_.size(); ++i) {
        if (ports_[i].first >= 0) {
            watched[ports_[i]] = i;
        }
    }
    // Digital devices (SN-203): their nets follow the firmware at once.
    DeviceBus bus;
    if (!options_.devices.empty()) {
        if (!bus.init(*machine, options_.devices, options_.nets, error)) {
            fail(error);
            return;
        }
        machine->set_pin_hook([&bus](int port, int pin, std::uint64_t time_ns) {
            bus.board_pin_changed(port, pin, time_ns);
        });
        bus.start(0);
    }
    // Debugging (SN-180): the GDB server lives on this thread, which owns the machine.
    std::unique_ptr<emu::GdbServer> gdb;
    if (options_.gdb_port >= 0) {
        gdb = std::make_unique<emu::GdbServer>(*machine);
        if (!gdb->start(static_cast<std::uint16_t>(options_.gdb_port), error)) {
            fail("Debugging: " + error);
            return;
        }
        gdb->wait_for_client(true);
        std::lock_guard lock(mutex_);
        status_.gdb_port = gdb->port();
        status_.debug_waiting = status_.debug_halted = true;
    }
    bool gdb_may_run = !gdb;
    auto last_service = std::chrono::steady_clock::now();
    // Talks to the debugger: every 2 ms while running, continuously while halted.
    auto service_debugger = [&] {
        const auto wall = std::chrono::steady_clock::now();
        if (gdb_may_run && wall - last_service < std::chrono::milliseconds(2)) {
            return;
        }
        last_service = wall;
        const bool was = gdb_may_run;
        gdb_may_run = gdb->service(gdb_may_run ? 0 : 20);
        std::lock_guard lock(mutex_);
        status_.debug_waiting = gdb->waiting();
        status_.debug_halted = !gdb_may_run;
        if (!was && gdb_may_run) {
            ++status_.debug_resumes;
        }
    };
    machine->set_analog_source([this, &watched](int port, int pin, std::uint64_t time_ns) {
        const auto it = watched.find({port, pin});
        return it == watched.end() ? 0.0 : analog_value(it->second, time_ns);
    });

    // Firmware events of a run step into the pin history, the level logs and the terminal.
    auto collect = [&] {
        for (const auto& e : machine->take_pin_events()) {
            const auto it = watched.find({e.port, e.pin});
            if (it == watched.end()) {
                continue;
            }
            const std::size_t i = it->second;
            PinState state = pin_state(e.mode, e.cnf, e.drive_high, e.pull_up, e.level);
            auto& history = history_[i];
            history.push_back({std::max(e.time_ns, history.back().first), state});
            trim_history(history, history_keep_ns);
            auto& log = levels_[i];
            if (log.back().second != e.level) {
                log.emplace_back(e.time_ns, e.level);
                if (log.size() > log_keep) {
                    log.erase(log.begin(), log.begin() + static_cast<std::ptrdiff_t>(log_keep / 10));
                }
            }
        }
        for (const auto& b : machine->take_uart_bytes()) {
            for (auto& [name, log] : uarts_) {
                if (log.number == b.usart) {
                    log.bytes += static_cast<char>(b.value);
                    log.times.push_back(b.time_ns);
                    ++log.total;
                    if (log.bytes.size() > uart_keep) {
                        const std::size_t drop = log.bytes.size() - uart_keep;
                        log.bytes.erase(0, drop);
                        log.times.erase(log.times.begin(), log.times.begin() + static_cast<std::ptrdiff_t>(drop));
                    }
                }
            }
        }
        for (const auto& d : machine->take_diagnostics()) {
            if (diagnostics_.size() < 100) {
                diagnostics_.push_back(d.text);
            }
        }
        // Device outputs: drives into the circuit and logic lanes, like board pins.
        for (const auto& change : bus.take_changes()) {
            const std::size_t i = device_pins_[change.device][change.pin];
            PinState state;
            state.mode = change.output.driven ? PinMode::Output : PinMode::Input;
            state.open_drain = change.output.open_drain;
            state.level = change.output.driven && change.output.high;
            state.pull = !change.output.driven && change.output.pull_up ? Pull::Up : Pull::None;
            state.strength = change.output.strength;
            auto& history = history_[i];
            history.push_back({std::max(change.time_ns, history.back().first), state});
            trim_history(history, history_keep_ns);
            auto& log = levels_[i];
            if (change.output.driven && log.back().second != change.output.high) {
                log.emplace_back(std::max(change.time_ns, log.back().first), change.output.high);
                if (log.size() > log_keep) {
                    log.erase(log.begin(), log.begin() + static_cast<std::ptrdiff_t>(log_keep / 10));
                }
            }
        }
        for (auto& text : bus.take_messages()) {
            if (diagnostics_.size() < 100) {
                diagnostics_.push_back(std::move(text));
            }
        }
        for (std::size_t d = 0; d < bus.device_count(); ++d) {
            auto text = bus.text(d);
            if (!text.empty()) {
                device_texts_[options_.devices[d].component] = std::move(text);
            }
            // Memory chips: a copy whenever the contents changed.
            const std::uint64_t version = bus.memory_version(d);
            const auto known = memories_.find(options_.devices[d].component);
            if (known == memories_.end() || known->second.version != version) {
                auto bytes = bus.memory(d);
                if (!bytes.empty()) {
                    memories_[options_.devices[d].component] = {version, std::move(bytes)};
                }
            }
        }
    };

    {
        std::lock_guard lock(mutex_);
        collect();
        sample_pins(*machine, 0);
        for (std::size_t i = 0; i < ports_.size(); ++i) {
            if (ports_[i].first >= 0) {
                levels_[i].front().second = machine->pin_level(ports_[i].first, ports_[i].second);
            }
        }
        status_.state = McuState::Running;
        status_.startup_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    }
    changed_.notify_all();

    const std::uint64_t window_ns = std::max<std::uint64_t>(options_.chunk_us, 1) * 1000;
    auto boundary_below = [window_ns](std::uint64_t t) { return t / window_ns * window_ns; };
    auto boundary_above = [window_ns](std::uint64_t t) { return (t + window_ns - 1) / window_ns * window_ns; };
    std::deque<InputChange> pending; // circuit inputs not applied yet, in time order
    while (!stop_) {
        if (gdb) {
            service_debugger();
            if (!gdb_may_run) {
                changed_.notify_all();
                continue; // halted: the firmware's time stands still, and so does the circuit
            }
        }
        std::uint64_t limit = 0;
        std::vector<std::pair<int, std::string>> to_send;
        {
            std::unique_lock lock(mutex_);
            const std::uint64_t now = machine->time_ns();
            changed_.wait_for(lock, std::chrono::milliseconds(20), [&] {
                bool sending = false;
                for (const auto& [name, log] : uarts_) {
                    sending = sending || !log.pending.empty();
                }
                return stop_ || reset_ || boundary_below(limit_ns_) > now || !input_queue_.empty() || sending
                    || !parameter_changes_.empty() || !memory_loads_.empty();
            });
            for (auto& [component, bytes] : std::exchange(memory_loads_, {})) {
                for (std::size_t d = 0; d < options_.devices.size(); ++d) {
                    if (options_.devices[d].component == component) {
                        bus.load_memory(d, bytes);
                    }
                }
            }
            for (const auto& change : std::exchange(parameter_changes_, {})) {
                for (std::size_t d = 0; d < options_.devices.size(); ++d) {
                    if (options_.devices[d].component == change.component) {
                        bus.set_parameter(d, change.name, change.value, now);
                    }
                }
            }
            limit = boundary_below(limit_ns_);
            for (auto change : input_queue_) {
                change.apply_ns = boundary_above(change.time_ns);
                pending.push_back(change);
            }
            input_queue_.clear();
            for (auto& [name, log] : uarts_) {
                if (!log.pending.empty()) {
                    to_send.emplace_back(log.number, std::exchange(log.pending, {}));
                }
            }
        }
        if (stop_) {
            break;
        }
        if (reset_.exchange(false)) {
            machine->reset(emu::ResetKind::Pin); // the board's RESET button pulls NRST low
            std::lock_guard lock(mutex_);
            ++status_.resets;
            collect();
        }
        for (const auto& [number, text] : to_send) {
            for (const char c : text) {
                machine->uart_receive(number, static_cast<std::uint8_t>(c));
            }
        }
        std::stable_sort(pending.begin(), pending.end(),
            [](const InputChange& a, const InputChange& b) { return a.apply_ns < b.apply_ns; });

        // Inputs apply at their window boundary; the firmware stops there for them.
        auto apply_due = [&] {
            const std::uint64_t now = machine->time_ns();
            while (!pending.empty() && pending.front().apply_ns <= now) {
                const auto& change = pending.front();
                const auto [port, pin] = ports_[change.pin];
                // A pin on a device net takes its level from the bus, not from the circuit.
                if (port >= 0 && !bus.owns_board_pin(port, pin)) {
                    machine->set_pin_input(port, pin, change.level, change.changed_ns);
                } else if (const auto device = device_of_pin_.find(change.pin); device != device_of_pin_.end()) {
                    bus.set_circuit_level(device->second.first, device->second.second, change.level, now);
                }
                {
                    std::lock_guard lock(mutex_);
                    status_.max_input_delay_ns = std::max(status_.max_input_delay_ns, now - change.time_ns);
                    // Inputs from the circuit show in the board display and the logic lanes.
                    if (port >= 0) {
                        sample_pins(*machine, now);
                        auto& log = levels_[change.pin];
                        const bool level = machine->pin_level(port, pin);
                        if (log.back().second != level) {
                            log.emplace_back(std::max(now, log.back().first), level);
                        }
                    }
                }
                pending.pop_front();
            }
        };
        apply_due();
        if (limit > machine->time_ns()) {
            std::uint64_t target = std::min(limit, boundary_below(machine->time_ns()) + window_ns);
            if (!pending.empty() && pending.front().apply_ns < target) {
                target = std::max(pending.front().apply_ns, machine->time_ns() + 1);
            }
            machine->run_until_ns(target); // device events run inside, at their times
            if (gdb) {
                gdb->after_run();
                if (gdb->halted()) {
                    gdb_may_run = false; // a breakpoint
                    std::lock_guard lock(mutex_);
                    status_.debug_halted = true;
                }
            }
            apply_due();
        }
        {
            std::lock_guard lock(mutex_);
            collect();
            status_.time_ns = machine->time_ns();
        }
        changed_.notify_all();
    }
    machine->set_pin_hook(nullptr); // the bus goes first
    std::lock_guard lock(mutex_);
    status_.state = McuState::Stopped;
}

} // namespace simnodus::mcu
