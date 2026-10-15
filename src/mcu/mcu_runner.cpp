// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcu_runner.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <utility>

namespace simnodus::mcu {

namespace {

constexpr std::uint64_t aircr = 0xE000ED0C;
constexpr std::uint32_t aircr_sysresetreq = 0x05FA0004; // VECTKEY | SYSRESETREQ
constexpr std::uint64_t history_keep_us = 100000;

std::optional<std::uint64_t> port_base(const std::string& port)
{
    if (port == "gpioPortA") {
        return 0x40010800;
    }
    if (port == "gpioPortB") {
        return 0x40010C00;
    }
    if (port == "gpioPortC") {
        return 0x40011000;
    }
    return std::nullopt;
}

std::string key(const std::string& port, int number)
{
    return port + ':' + std::to_string(number);
}

} // namespace

std::optional<GpioPin> gpio_pin(const std::string& name)
{
    if (name.size() < 3 || name[0] != 'P' || name[1] < 'A' || name[1] > 'G') {
        return std::nullopt;
    }
    int number = 0;
    for (std::size_t i = 2; i < name.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
            return std::nullopt;
        }
        number = number * 10 + (name[i] - '0');
    }
    if (number > 15) {
        return std::nullopt;
    }
    return GpioPin{std::string("gpioPort") + name[1], number};
}

PinMode decode_pin_mode(std::uint32_t nibble)
{
    const std::uint32_t mode = nibble & 0x3u;
    const std::uint32_t cnf = (nibble >> 2) & 0x3u;
    if (mode == 0) {
        return cnf == 0 ? PinMode::Analog : PinMode::Input;
    }
    return (cnf & 0x2u) ? PinMode::Alternate : PinMode::Output;
}

PinState decode_pin_config(std::uint32_t nibble, bool odr_bit)
{
    PinState state;
    state.mode = decode_pin_mode(nibble);
    const std::uint32_t cnf = (nibble >> 2) & 0x3u;
    if (state.mode == PinMode::Input) {
        state.pull = cnf == 2 ? (odr_bit ? Pull::Up : Pull::Down) : Pull::None;
    } else {
        state.open_drain = (cnf & 0x1u) != 0;
    }
    return state;
}

bool schmitt(bool previous, double volts, const PinModel& model)
{
    if (volts >= model.input_high) {
        return true;
    }
    if (volts <= model.input_low) {
        return false;
    }
    return previous;
}

PinDrive pin_drive(const PinState& state, const PinModel& model)
{
    const double out = 1.0 / model.output_resistance;
    const double pull = 1.0 / model.pull_resistance;
    switch (state.mode) {
    case PinMode::Output:
    case PinMode::Alternate:
        if (state.open_drain) {
            return state.level ? PinDrive{} : PinDrive{out, 0.0};
        }
        return {out, state.level ? model.supply * out : 0.0};
    case PinMode::Input:
        if (state.pull == Pull::Up) {
            return {pull, model.supply * pull};
        }
        if (state.pull == Pull::Down) {
            return {pull, 0.0};
        }
        return {};
    case PinMode::Analog:
    case PinMode::Unknown:
        break;
    }
    return {};
}

McuRunner::McuRunner(McuRunOptions options) : options_(std::move(options))
{
    history_.resize(options_.pins.size());
    inputs_.resize(options_.pins.size());
    levels_.assign(options_.pins.size(), {{0, false}});
    for (const auto& uart : options_.uarts) {
        uarts_[uart] = {};
    }
    for (std::size_t i = 0; i < options_.pins.size(); ++i) {
        if (const auto pin = gpio_pin(options_.pins[i])) {
            event_pin_[key(pin->port, pin->number)] = i;
        }
        history_[i].push_back({0, PinState{}});
    }
}

McuRunner::~McuRunner()
{
    stop();
}

void McuRunner::start()
{
    {
        std::lock_guard lock(mutex_);
        status_ = {};
        status_.state = McuState::Starting;
    }
    stop_ = false;
    worker_ = std::thread(&McuRunner::work, this);
}

bool McuRunner::wait_ready(const std::atomic<bool>& cancel)
{
    std::unique_lock lock(mutex_);
    while (status_.state == McuState::Starting && !cancel) {
        changed_.wait_for(lock, std::chrono::milliseconds(20));
    }
    return status_.state == McuState::Running;
}

void McuRunner::set_limit_us(std::uint64_t limit)
{
    {
        std::lock_guard lock(mutex_);
        limit_us_ = std::max(limit_us_, limit);
    }
    changed_.notify_all();
}

bool McuRunner::wait_time_us(std::uint64_t time_us, const std::atomic<bool>& cancel)
{
    std::unique_lock lock(mutex_);
    while (status_.time_us < time_us && status_.state == McuState::Running && !cancel) {
        changed_.wait_for(lock, std::chrono::milliseconds(5));
    }
    return status_.time_us >= time_us;
}

void McuRunner::set_input(std::size_t pin, bool level, std::uint64_t time_us)
{
    std::lock_guard lock(mutex_);
    if (pin < inputs_.size() && inputs_[pin].desired != level) {
        inputs_[pin].desired = level;
        inputs_[pin].since_us = time_us;
    }
}

void McuRunner::uart_send(const std::string& uart, const std::string& bytes)
{
    std::lock_guard lock(mutex_);
    if (const auto it = uarts_.find(uart); it != uarts_.end()) {
        it->second.pending += bytes;
    }
}

std::uint64_t McuRunner::uart_received(
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

void McuRunner::request_reset()
{
    reset_ = true;
    changed_.notify_all();
}

void McuRunner::stop()
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

McuStatus McuRunner::status() const
{
    std::lock_guard lock(mutex_);
    return status_;
}

std::map<std::string, PinState> McuRunner::pins() const
{
    std::lock_guard lock(mutex_);
    std::map<std::string, PinState> result;
    for (std::size_t i = 0; i < options_.pins.size(); ++i) {
        result[options_.pins[i]] = history_[i].back().second;
    }
    return result;
}

std::vector<PinDrive> McuRunner::drives_at(std::uint64_t time_us)
{
    std::lock_guard lock(mutex_);
    std::vector<PinDrive> drives(options_.pins.size());
    for (std::size_t i = 0; i < history_.size(); ++i) {
        auto& history = history_[i];
        // Last entry at or before time_us (the first entry is at time 0).
        auto it = std::upper_bound(history.begin(), history.end(), time_us,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        const auto current = it == history.begin() ? history.begin() : std::prev(it);
        drives[i] = pin_drive(current->second, options_.pin_model);
        // Keep one entry before the window that the circuit may still ask for.
        const std::uint64_t horizon = time_us > history_keep_us ? time_us - history_keep_us : 0;
        auto keep = std::upper_bound(history.begin(), history.end(), horizon,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        if (keep != history.begin() && std::prev(keep) != history.begin()) {
            history.erase(history.begin(), std::prev(keep));
        }
    }
    return drives;
}

PinLog McuRunner::pin_log(std::size_t pin, std::uint64_t from_us) const
{
    std::lock_guard lock(mutex_);
    PinLog result;
    if (pin >= levels_.size()) {
        return result;
    }
    const auto& log = levels_[pin];
    auto it = std::upper_bound(log.begin(), log.end(), from_us,
        [](std::uint64_t t, const auto& entry) { return t < entry.first; });
    result.initial = it == log.begin() ? log.front().second : std::prev(it)->second;
    result.changes.assign(it, log.end());
    return result;
}

void McuRunner::fail(const std::string& message)
{
    {
        std::lock_guard lock(mutex_);
        status_.state = McuState::Failed;
        status_.message = message;
    }
    changed_.notify_all();
}

bool McuRunner::read_configs(RenodeEngine& engine, std::vector<PinState>& configs)
{
    std::map<std::uint64_t, std::uint32_t> registers;
    auto reg = [&](std::uint64_t address) -> std::optional<std::uint32_t> {
        if (const auto it = registers.find(address); it != registers.end()) {
            return it->second;
        }
        const auto value = engine.read32(address);
        if (value) {
            registers[address] = *value;
        }
        return value;
    };
    configs.assign(options_.pins.size(), PinState{});
    for (std::size_t i = 0; i < options_.pins.size(); ++i) {
        const auto pin = gpio_pin(options_.pins[i]);
        const auto base = pin ? port_base(pin->port) : std::nullopt;
        if (!base) {
            continue;
        }
        const auto cr = reg(*base + (pin->number >= 8 ? 4u : 0u));
        const auto odr = reg(*base + 0x0C);
        if (!cr || !odr) {
            return false;
        }
        const std::uint32_t nibble = (*cr >> (4 * (pin->number % 8))) & 0xFu;
        configs[i] = decode_pin_config(nibble, ((*odr >> pin->number) & 1u) != 0);
    }
    return true;
}

void McuRunner::apply_configs(std::uint64_t time_us, const std::vector<PinState>& configs)
{
    for (std::size_t i = 0; i < configs.size() && i < history_.size(); ++i) {
        auto& history = history_[i];
        if (history.back().second.same_config(configs[i])) {
            continue;
        }
        // The new configuration holds from `time_us`, also for levels recorded after it.
        auto at = std::upper_bound(history.begin(), history.end(), time_us,
            [](std::uint64_t t, const auto& entry) { return t < entry.first; });
        PinState state = std::prev(at)->second;
        state.mode = configs[i].mode;
        state.open_drain = configs[i].open_drain;
        state.pull = configs[i].pull;
        at = history.insert(at, {time_us, state});
        for (++at; at != history.end(); ++at) {
            at->second.mode = state.mode;
            at->second.open_drain = state.open_drain;
            at->second.pull = state.pull;
        }
    }
}

void McuRunner::record_level(std::size_t pin, std::uint64_t time_us, bool level)
{
    auto& history = history_[pin];
    PinState state = history.back().second;
    if (state.level == level) {
        return;
    }
    state.level = level;
    history.push_back({std::max(time_us, history.back().first), state});
    auto& log = levels_[pin];
    log.emplace_back(history.back().first, level);
    if (log.size() > 50000) {
        log.erase(log.begin(), log.begin() + 5000);
    }
}

void McuRunner::work()
{
    // The engine lives in this thread: on Linux, Renode dies with the thread that started it.
    std::string error;
    const auto begin = std::chrono::steady_clock::now();
    MachineOptions machine = options_.machine;
    machine.cancel = &stop_;
    auto engine = RenodeEngine::start(machine, error);
    if (!engine && stop_) {
        std::lock_guard lock(mutex_);
        status_.state = McuState::Stopped;
        return;
    }
    if (!engine) {
        fail(error);
        return;
    }
    auto read_levels = [&](std::uint64_t time_us) {
        for (std::size_t i = 0; i < options_.pins.size(); ++i) {
            const auto pin = gpio_pin(options_.pins[i]);
            if (const auto level = pin ? engine->gpio(pin->port, pin->number) : std::nullopt) {
                std::lock_guard lock(mutex_);
                record_level(i, time_us, *level);
            }
        }
    };
    for (const auto& name : options_.pins) {
        const auto pin = gpio_pin(name);
        if (!pin || !engine->watch_gpio(pin->port, pin->number)) {
            fail("Cannot watch pin " + name + ": " + engine->error());
            engine->stop();
            return;
        }
    }
    for (const auto& uart : options_.uarts) {
        if (!engine->watch_uart(uart)) {
            fail("Cannot open " + uart + ": " + engine->error());
            engine->stop();
            return;
        }
    }
    read_levels(0);
    std::vector<PinState> configs;
    if (read_configs(*engine, configs)) {
        std::lock_guard lock(mutex_);
        apply_configs(0, configs);
    }
    {
        std::lock_guard lock(mutex_);
        status_.state = McuState::Running;
        status_.startup_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    }
    changed_.notify_all();

    std::uint64_t now = 0;
    std::uint64_t last_config_us = 0;
    bool after_reset = false;
    // Circuit levels of input pins, before the firmware runs on (sampled exchange, E-03).
    auto apply_inputs = [&] {
        std::vector<std::pair<std::size_t, bool>> changes;
        {
            std::lock_guard lock(mutex_);
            for (std::size_t i = 0; i < inputs_.size(); ++i) {
                auto& in = inputs_[i];
                if (in.desired != in.applied && history_[i].back().second.mode == PinMode::Input) {
                    changes.emplace_back(i, in.desired);
                    in.applied = in.desired;
                    if (now > in.since_us) {
                        status_.max_input_delay_us = std::max(status_.max_input_delay_us, now - in.since_us);
                    }
                    record_level(i, now, in.desired);
                }
            }
        }
        for (const auto& [i, level] : changes) {
            const auto pin = gpio_pin(options_.pins[i]);
            if (!pin || !engine->set_gpio(pin->port, pin->number, level)) {
                return false;
            }
        }
        return true;
    };
    while (!stop_) {
        if (reset_.exchange(false)) {
            // SYSRESETREQ: Renode resets the machine; the firmware restarts from its reset vector.
            if (!engine->write32(aircr, aircr_sysresetreq)) {
                break;
            }
            {
                std::lock_guard lock(mutex_);
                ++status_.resets;
            }
            after_reset = true;
        }
        std::uint64_t limit = 0;
        {
            std::unique_lock lock(mutex_);
            if (limit_us_ <= now && !stop_ && !reset_) {
                changed_.wait_for(lock, std::chrono::milliseconds(5));
                continue;
            }
            limit = limit_us_;
        }
        if (limit <= now) {
            continue;
        }
        if (!apply_inputs()) {
            break;
        }
        std::vector<std::pair<std::string, std::string>> sends;
        {
            std::lock_guard lock(mutex_);
            for (auto& [name, log] : uarts_) {
                if (!log.pending.empty()) {
                    sends.emplace_back(name, std::exchange(log.pending, {}));
                }
            }
        }
        bool sent = true;
        for (const auto& [name, bytes] : sends) {
            sent = sent && engine->write_uart(name, std::vector<std::uint8_t>(bytes.begin(), bytes.end()));
        }
        if (!sent) {
            break;
        }
        const std::uint64_t start = now;
        const std::uint64_t step = std::min(limit - now, options_.chunk_us);
        if (!engine->run_for(step)) {
            break;
        }
        now += step;
        const auto events = engine->take_gpio_events();
        const bool reset_done = after_reset;
        if (after_reset) {
            // The reset (done at Renode's next synchronization point) clears the
            // outputs without events: take the levels as they are now.
            read_levels(start);
            after_reset = false;
        }
        // Configuration every config_interval_us of virtual time, effective from
        // the previous read (firmware sets the mode before driving the pin); read
        // before the new time is published so the circuit never sees a stale one.
        bool configs_ok = false;
        std::uint64_t config_from = start;
        if (now - last_config_us >= options_.config_interval_us || reset_done) {
            configs_ok = read_configs(*engine, configs);
            config_from = last_config_us;
            last_config_us = now;
        }
        {
            std::lock_guard lock(mutex_);
            if (configs_ok) {
                apply_configs(config_from, configs);
            }
            for (const auto& e : events) {
                if (const auto it = event_pin_.find(key(e.port, e.pin)); it != event_pin_.end()) {
                    record_level(it->second, std::max(e.time_us, start), e.state);
                }
            }
            for (const auto& b : engine->take_uart_bytes()) {
                if (const auto it = uarts_.find(b.uart); it != uarts_.end()) {
                    auto& log = it->second;
                    log.bytes.push_back(static_cast<char>(b.value));
                    log.times.push_back(b.time_us);
                    ++log.total;
                    if (log.bytes.size() > (1u << 20)) {
                        const std::size_t drop = log.bytes.size() - (1u << 20) + 4096;
                        log.bytes.erase(0, drop);
                        log.times.erase(log.times.begin(), log.times.begin() + static_cast<std::ptrdiff_t>(drop));
                    }
                }
            }
            status_.time_us = now;
        }
        changed_.notify_all();
    }
    if (!stop_) {
        const std::string reason = engine->error();
        const std::string tail = engine->log_tail(8);
        engine->stop();
        fail("The microcontroller engine stopped: " + reason + "\n" + tail);
        return;
    }
    engine->stop();
    std::lock_guard lock(mutex_);
    status_.state = McuState::Stopped;
}

} // namespace simnodus::mcu
