// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "native_mcu_runner.hpp"

#include "emu/elf.hpp"
#include "emu/stm32f103.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>

namespace simnodus::mcu {

namespace {

constexpr std::uint64_t history_keep_ns = 100000000; // as RenodeMcuRunner: 100 ms
constexpr std::size_t uart_keep = 1 << 20;
constexpr std::size_t log_keep = 50000;

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
    history_.resize(options_.pins.size());
    levels_.assign(options_.pins.size(), {{0, false}});
    input_desired_.assign(options_.pins.size(), false);
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

void NativeMcuRunner::set_input(std::size_t pin, bool level, std::uint64_t time_ns)
{
    {
        std::lock_guard lock(mutex_);
        if (pin >= input_desired_.size() || input_desired_[pin] == level) {
            return;
        }
        input_desired_[pin] = level;
        input_queue_.push_back({pin, level, time_ns});
    }
    changed_.notify_all();
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
        drives[i] = pin_drive(current->second, options_.pin_model);
        const std::uint64_t horizon = time_ns > history_keep_ns ? time_ns - history_keep_ns : 0;
        auto keep = std::upper_bound(history.begin(), history.end(), horizon,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        if (keep != history.begin() && std::prev(keep) != history.begin()) {
            history.erase(history.begin(), std::prev(keep));
        }
    }
    return drives;
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
        }
    }
}

void NativeMcuRunner::work()
{
    const auto begin = std::chrono::steady_clock::now();
    std::string error;
    const auto image = emu::read_elf(options_.machine.firmware, error);
    auto machine = std::make_unique<emu::Stm32F103>();
    if (!image || !machine->load(*image, error)) {
        fail(error);
        return;
    }
    std::map<std::pair<int, int>, std::size_t> watched;
    for (std::size_t i = 0; i < ports_.size(); ++i) {
        watched[ports_[i]] = i;
    }

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

    const std::uint64_t chunk_ns = std::max<std::uint64_t>(options_.chunk_us, 1) * 1000;
    std::deque<InputChange> pending; // circuit inputs not applied yet, in time order
    while (!stop_) {
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
                return stop_ || reset_ || limit_ns_ > now || !input_queue_.empty() || sending;
            });
            limit = limit_ns_;
            for (auto& change : input_queue_) {
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
            [](const InputChange& a, const InputChange& b) { return a.time_ns < b.time_ns; });

        // Inputs whose circuit time the firmware has reached apply now; later ones
        // apply exactly at their time.
        auto apply_due = [&] {
            const std::uint64_t now = machine->time_ns();
            while (!pending.empty() && pending.front().time_ns <= now) {
                const auto& change = pending.front();
                const auto [port, pin] = ports_[change.pin];
                if (port >= 0) {
                    machine->set_pin_input(port, pin, change.level);
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
            std::uint64_t target = std::min(limit, machine->time_ns() + chunk_ns);
            if (!pending.empty() && pending.front().time_ns < target) {
                target = std::max(pending.front().time_ns, machine->time_ns() + 1);
            }
            machine->run_until_ns(target);
            apply_due();
        }
        {
            std::lock_guard lock(mutex_);
            collect();
            status_.time_ns = machine->time_ns();
        }
        changed_.notify_all();
    }
    std::lock_guard lock(mutex_);
    status_.state = McuState::Stopped;
}

} // namespace simnodus::mcu
