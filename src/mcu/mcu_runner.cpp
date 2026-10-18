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

std::uint64_t usart_base(const std::string& uart)
{
    if (uart == "usart1") {
        return 0x40013800;
    }
    if (uart == "usart2") {
        return 0x40004400;
    }
    if (uart == "usart3") {
        return 0x40004800;
    }
    return 0;
}

constexpr std::uint32_t usart_cr1_ue = 1u << 13; // USART enable
constexpr std::uint32_t usart_cr1_te = 1u << 3;  // transmitter enable

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

RenodeMcuRunner::RenodeMcuRunner(McuRunOptions options) : options_(std::move(options))
{
    history_.resize(options_.pins.size());
    inputs_.resize(options_.pins.size());
    levels_.assign(options_.pins.size(), {{0, false}});
    tx_usart_.assign(options_.pins.size(), 0);
    for (const auto& [uart, tx] : options_.uart_tx_pins) {
        const auto it = std::find(options_.pins.begin(), options_.pins.end(), tx);
        if (it != options_.pins.end()) {
            tx_usart_[static_cast<std::size_t>(it - options_.pins.begin())] = usart_base(uart);
        }
    }
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

RenodeMcuRunner::~RenodeMcuRunner()
{
    stop();
}

void RenodeMcuRunner::start()
{
    {
        std::lock_guard lock(mutex_);
        status_ = {};
        status_.state = McuState::Starting;
    }
    stop_ = false;
    worker_ = std::thread(&RenodeMcuRunner::work, this);
}

bool RenodeMcuRunner::wait_ready(const std::atomic<bool>& cancel)
{
    std::unique_lock lock(mutex_);
    while (status_.state == McuState::Starting && !cancel) {
        changed_.wait_for(lock, std::chrono::milliseconds(20));
    }
    return status_.state == McuState::Running;
}

void RenodeMcuRunner::set_limit_ns(std::uint64_t limit)
{
    {
        std::lock_guard lock(mutex_);
        limit_us_ = std::max(limit_us_, limit / 1000);
    }
    changed_.notify_all();
}

bool RenodeMcuRunner::wait_time_ns(std::uint64_t time_ns, const std::atomic<bool>& cancel)
{
    std::unique_lock lock(mutex_);
    while (status_.time_ns < time_ns && status_.state == McuState::Running && !cancel) {
        changed_.wait_for(lock, std::chrono::milliseconds(5));
    }
    return status_.time_ns >= time_ns;
}

void RenodeMcuRunner::set_input(std::size_t pin, bool level, std::uint64_t time_ns, std::uint64_t /*changed_ns*/)
{
    const std::uint64_t time_us = time_ns / 1000;
    std::lock_guard lock(mutex_);
    if (pin < inputs_.size() && inputs_[pin].desired != level) {
        inputs_[pin].desired = level;
        inputs_[pin].since_us = time_us;
        input_queue_.push_back({pin, level, time_us});
    }
}

void RenodeMcuRunner::uart_send(const std::string& uart, const std::string& bytes)
{
    std::lock_guard lock(mutex_);
    if (const auto it = uarts_.find(uart); it != uarts_.end()) {
        it->second.pending += bytes;
    }
}

std::uint64_t RenodeMcuRunner::uart_received(
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
            for (auto t = log.times.begin() + static_cast<std::ptrdiff_t>(offset); t != log.times.end(); ++t) {
                times->push_back(*t * 1000);
            }
        }
    }
    return log.total;
}

void RenodeMcuRunner::request_reset()
{
    reset_ = true;
    changed_.notify_all();
}

void RenodeMcuRunner::stop()
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

McuStatus RenodeMcuRunner::status() const
{
    std::lock_guard lock(mutex_);
    return status_;
}

std::map<std::string, PinState> RenodeMcuRunner::pins() const
{
    std::lock_guard lock(mutex_);
    std::map<std::string, PinState> result;
    for (std::size_t i = 0; i < options_.pins.size(); ++i) {
        result[options_.pins[i]] = history_[i].back().second;
    }
    return result;
}

std::vector<PinDrive> RenodeMcuRunner::drives_at(std::uint64_t time_ns)
{
    const std::uint64_t time_us = time_ns / 1000;
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

PinLog RenodeMcuRunner::pin_log(std::size_t pin, std::uint64_t from_ns) const
{
    const std::uint64_t from_us = from_ns / 1000;
    std::lock_guard lock(mutex_);
    PinLog result;
    if (pin >= levels_.size()) {
        return result;
    }
    const auto& log = levels_[pin];
    auto it = std::upper_bound(log.begin(), log.end(), from_us,
        [](std::uint64_t t, const auto& entry) { return t < entry.first; });
    result.initial = it == log.begin() ? log.front().second : std::prev(it)->second;
    for (; it != log.end(); ++it) {
        result.changes.emplace_back(it->first * 1000, it->second);
    }
    return result;
}

void RenodeMcuRunner::fail(const std::string& message)
{
    {
        std::lock_guard lock(mutex_);
        status_.state = McuState::Failed;
        status_.message = message;
    }
    changed_.notify_all();
}

bool RenodeMcuRunner::read_configs(RenodeEngine& engine, std::vector<PinState>& configs)
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
        if (tx_usart_[i] != 0 && configs[i].mode == PinMode::Alternate) {
            // The level of a TX pin is the USART's: high at rest (bits are not simulated).
            const auto cr1 = reg(tx_usart_[i] + 0x0C);
            if (!cr1) {
                return false;
            }
            const std::uint32_t on = usart_cr1_ue | usart_cr1_te;
            configs[i].level = (*cr1 & on) == on;
        }
    }
    return true;
}

void RenodeMcuRunner::apply_configs(std::uint64_t time_us, const std::vector<PinState>& configs)
{
    for (std::size_t i = 0; i < configs.size() && i < history_.size(); ++i) {
        auto& history = history_[i];
        if (history.back().second.same_config(configs[i])) {
            if (tx_usart_[i] != 0 && configs[i].mode == PinMode::Alternate) {
                record_level(i, time_us, configs[i].level);
            }
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
        if (tx_usart_[i] != 0 && configs[i].mode == PinMode::Alternate) {
            record_level(i, time_us, configs[i].level);
        }
    }
}

void RenodeMcuRunner::record_level(std::size_t pin, std::uint64_t time_us, bool level)
{
    auto& history = history_[pin];
    PinState state = history.back().second;
    if (state.level == level) {
        return;
    }
    state.level = level;
    history.push_back({std::max(time_us, history.back().first), state});
    trim_history(history, history_keep_us);
    auto& log = levels_[pin];
    log.emplace_back(history.back().first, level);
    if (log.size() > 50000) {
        log.erase(log.begin(), log.begin() + 5000);
    }
}

void RenodeMcuRunner::work()
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
    // Records what Renode reported. Events of an earlier chunk can arrive in a
    // later run (held by the server, ADR 0085): they keep their own time, which
    // the circuit has not used yet (the runner reports chunks one chunk late).
    auto absorb = [&] {
        const auto events = engine->take_gpio_events();
        std::lock_guard lock(mutex_);
        for (const auto& e : events) {
            if (const auto it = event_pin_.find(key(e.port, e.pin)); it != event_pin_.end()) {
                record_level(it->second, e.time_us, e.state);
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
    };
    // Applies a circuit level to an input pin; `applied` tells whether it changed the pin.
    auto set_pin = [&](std::size_t i, bool level, std::uint64_t since, bool* applied = nullptr) {
        if (applied) {
            *applied = false;
        }
        {
            std::lock_guard lock(mutex_);
            if (history_[i].back().second.mode != PinMode::Input || inputs_[i].applied == level) {
                return true; // outputs ignore the circuit; the wanted level is applied if the pin becomes an input
            }
            if (applied) {
                *applied = true;
            }
            inputs_[i].applied = level;
            if (now > since) {
                status_.max_input_delay_ns = std::max(status_.max_input_delay_ns, (now - since) * 1000);
            }
            record_level(i, now, level);
        }
        const auto pin = gpio_pin(options_.pins[i]);
        return pin && engine->set_gpio(pin->port, pin->number, level);
    };
    auto apply_inputs = [&] {
        std::vector<InputChange> queue;
        {
            std::lock_guard lock(mutex_);
            queue.swap(input_queue_);
        }
        for (std::size_t k = 0; k < queue.size(); ++k) {
            bool applied = false;
            if (!set_pin(queue[k].pin, queue[k].level, queue[k].time_us, &applied)) {
                return false;
            }
            // Keep the spacing to the next change (at most one chunk), so that the
            // firmware sees every pulse with its width, only later. Changes of
            // output pins (their own level seen by the circuit) need no spacing.
            if (applied && k + 1 < queue.size() && queue[k + 1].time_us > queue[k].time_us) {
                const std::uint64_t gap = std::min(queue[k + 1].time_us - queue[k].time_us, options_.chunk_us);
                if (!engine->run_for(gap)) {
                    return false;
                }
                now += gap;
                absorb();
            }
        }
        // Wanted levels of pins that became inputs meanwhile.
        for (std::size_t i = 0; i < inputs_.size(); ++i) {
            bool level = false;
            std::uint64_t since = 0;
            {
                std::lock_guard lock(mutex_);
                level = inputs_[i].desired;
                since = inputs_[i].since_us;
            }
            if (!set_pin(i, level, since)) {
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
        // The runner may run one chunk past the limit: it reports only the time
        // before its last chunk as complete (see below), and the circuit must be
        // able to reach the limit.
        std::uint64_t limit = 0;
        {
            std::unique_lock lock(mutex_);
            if (limit_us_ + options_.chunk_us <= now && !stop_ && !reset_) {
                changed_.wait_for(lock, std::chrono::milliseconds(5));
                continue;
            }
            limit = limit_us_ + options_.chunk_us;
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
        absorb();
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
        // Renode can raise events of a chunk after the RunFor returned (its CPU
        // thread finishes the quantum; seen as a whole chunk of PA1 edges arriving
        // late). The server holds them for the next RunFor, so only the time
        // before this chunk is complete.
        const std::uint64_t complete = start;
        {
            std::lock_guard lock(mutex_);
            if (configs_ok) {
                apply_configs(config_from, configs);
            }
            status_.time_ns = complete * 1000;
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
