// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcu_runner.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>

namespace simnodus::mcu {

namespace {

constexpr std::uint64_t aircr = 0xE000ED0C;
constexpr std::uint32_t aircr_sysresetreq = 0x05FA0004; // VECTKEY | SYSRESETREQ

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
        return PinMode::Input; // see the header: CNF 00 is also Renode's reset state
    }
    return (cnf & 0x2u) ? PinMode::Alternate : PinMode::Output;
}

McuRunner::McuRunner(McuRunOptions options) : options_(std::move(options))
{
    for (const auto& name : options_.pins) {
        if (const auto pin = gpio_pin(name)) {
            pins_[name] = {};
            event_pin_[key(pin->port, pin->number)] = name;
        }
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
    return pins_;
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

void McuRunner::read_modes(RenodeEngine& engine)
{
    std::map<std::uint64_t, std::uint32_t> registers;
    std::map<std::string, PinState> update;
    for (const auto& name : options_.pins) {
        const auto pin = gpio_pin(name);
        const auto base = pin ? port_base(pin->port) : std::nullopt;
        if (!base) {
            continue;
        }
        const std::uint64_t address = *base + (pin->number >= 8 ? 4u : 0u);
        if (!registers.count(address)) {
            const auto value = engine.read32(address);
            if (!value) {
                return;
            }
            registers[address] = *value;
        }
        const std::uint32_t nibble = (registers[address] >> (4 * (pin->number % 8))) & 0xFu;
        update[name].mode = decode_pin_mode(nibble);
    }
    std::lock_guard lock(mutex_);
    for (const auto& [name, state] : update) {
        pins_[name].mode = state.mode;
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
    for (const auto& name : options_.pins) {
        const auto pin = gpio_pin(name);
        if (!pin || !engine->watch_gpio(pin->port, pin->number)) {
            fail("Cannot watch pin " + name + ": " + engine->error());
            engine->stop();
            return;
        }
        if (const auto level = engine->gpio(pin->port, pin->number)) {
            std::lock_guard lock(mutex_);
            pins_[name].level = *level;
        }
    }
    read_modes(*engine);
    {
        std::lock_guard lock(mutex_);
        status_.state = McuState::Running;
        status_.startup_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    }
    changed_.notify_all();

    std::uint64_t now = 0;
    auto last_modes = std::chrono::steady_clock::now();
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
            read_modes(*engine);
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
        const std::uint64_t step = std::min(limit - now, options_.chunk_us);
        if (!engine->run_for(step)) {
            break;
        }
        now += step;
        const auto events = engine->take_gpio_events();
        {
            std::lock_guard lock(mutex_);
            for (const auto& e : events) {
                if (const auto it = event_pin_.find(key(e.port, e.pin)); it != event_pin_.end()) {
                    pins_[it->second].level = e.state;
                }
            }
            status_.time_us = now;
        }
        changed_.notify_all();
        if (std::chrono::steady_clock::now() - last_modes > std::chrono::milliseconds(50)) {
            read_modes(*engine);
            last_modes = std::chrono::steady_clock::now();
        }
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
