// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "sim/remote_session.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <utility>

namespace simnodus::sim {

using protocol::Reader;
using protocol::Type;
using protocol::Writer;

namespace {

// The engine keeps as much; the mirror keeps the same.
constexpr std::size_t uart_keep = 1 << 20;
constexpr std::size_t log_keep = 50000;

std::string lower(std::string s)
{
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

} // namespace

RemoteSession::RemoteSession(EngineLink& link, protocol::EngineSetup setup) : link_(link), setup_(std::move(setup))
{
    link_.on_message = [this](const std::string& payload) { receive(payload); };
    link_.on_lost = [this](const std::string& reason) { lost(reason); };
}

RemoteSession::~RemoteSession()
{
    stop();
    link_.on_message = nullptr;
    link_.on_lost = nullptr;
}

bool RemoteSession::start(RunRequest request, std::string& error)
{
    stop();
    if (!request.library) {
        error = "No component library.";
        return false;
    }
    // The same checks as SimulationSession::start, so that the circuit's
    // problems are known here without the engine.
    const bool interactive = request.schematic.settings().mode == model::SimulationMode::Interactive;
    netlist_ = netlist::generate_netlist(request.schematic, *request.library, {interactive, 0.0, request.current_probes});
    diagnostics_ = netlist_.diagnostics;
    if (!netlist_.ok()) {
        error = "The circuit has errors; see the Messages panel.";
        return false;
    }
    if (netlist_.elements.empty()) {
        error = "The schematic is empty.";
        return false;
    }

    status_ = {};
    latest_.clear();
    controls_.clear();
    vectors_.clear();
    rings_.clear();
    transient_ = {};
    boards_.clear();
    mcu_diagnostics_.clear();
    texts_.clear();
    ring_capacity_ = request.ring_capacity;

    if (!link_.open(error)) {
        return false;
    }
    lost_ = false;
    started_ = false;
    if (!link_.send(protocol::start_message(request, setup_)) || !wait([this] { return started_; }, start_timeout_ms_)) {
        error = lost_ ? lost_reason_ : "The simulation engine did not answer.";
        if (!lost_) {
            link_.kill();
        }
        status_ = {};
        return false;
    }
    if (!started_ok_) {
        error = started_error_;
        return false;
    }
    engine_run_ = true;
    return true;
}

void RemoteSession::pause()
{
    if (active()) {
        link_.send(Writer(Type::Pause).payload());
    }
}

void RemoteSession::resume()
{
    if (engine_run_) {
        link_.send(Writer(Type::Resume).payload());
    }
}

void RemoteSession::stop()
{
    if (!engine_run_) {
        return;
    }
    engine_run_ = false;
    stopped_ = false;
    if (!link_.send(Writer(Type::Stop).payload()) || !wait([this] { return stopped_; }, stop_timeout_ms_)) {
        if (!lost_) {
            link_.kill(); // a stuck engine: the next run starts a new one
        }
        if (status_.state == RunState::Running || status_.state == RunState::Paused) {
            status_.state = RunState::Stopped;
        }
    }
}

bool RemoteSession::active() const
{
    return status_.state == RunState::Running || status_.state == RunState::Paused;
}

void RemoteSession::set_speed(double speed)
{
    status_.speed = std::max(0.0, speed);
    if (engine_run_) {
        Writer w(Type::Speed);
        w.f64(status_.speed);
        link_.send(w.payload());
    }
}

void RemoteSession::set_control(model::Id component, double value)
{
    // Only the editor changes controls, so its value is the current one at once.
    if (controls_.count(component)) {
        controls_[component] = value;
    }
    if (engine_run_) {
        Writer w(Type::Control);
        w.u64(component);
        w.f64(value);
        link_.send(w.payload());
    }
}

std::optional<double> RemoteSession::control(model::Id component) const
{
    const auto it = controls_.find(component);
    return it == controls_.end() ? std::nullopt : std::optional<double>(it->second);
}

std::vector<Sample> RemoteSession::samples(const std::string& vector) const
{
    const auto it = rings_.find(lower(vector));
    return it == rings_.end() ? std::vector<Sample>{} : it->second->snapshot();
}

std::vector<Sample> RemoteSession::samples_since(const std::string& vector, double from, double bucket) const
{
    const auto it = rings_.find(lower(vector));
    return it == rings_.end() ? std::vector<Sample>{} : it->second->since(from, bucket);
}

std::vector<std::pair<double, bool>> RemoteSession::transitions_since(const std::string& vector, double from,
    double threshold, bool& initial) const
{
    initial = false;
    const auto it = rings_.find(lower(vector));
    return it == rings_.end() ? std::vector<std::pair<double, bool>>{} : it->second->transitions(from, threshold, initial);
}

std::optional<double> RemoteSession::latest(const std::string& vector) const
{
    const auto key = lower(vector);
    if (key == "0") {
        return 0.0;
    }
    const auto it = latest_.find(key);
    return it == latest_.end() ? std::nullopt : std::optional<double>(it->second);
}

std::optional<mcu::McuStatus> RemoteSession::mcu_status(model::Id component) const
{
    const auto it = boards_.find(component);
    return it == boards_.end() ? std::nullopt : it->second.status;
}

std::vector<std::pair<model::Id, std::string>> RemoteSession::take_mcu_diagnostics()
{
    return std::exchange(mcu_diagnostics_, {});
}

std::map<std::string, mcu::PinState> RemoteSession::mcu_pins(model::Id component) const
{
    const auto it = boards_.find(component);
    return it == boards_.end() ? std::map<std::string, mcu::PinState>{} : it->second.pins;
}

void RemoteSession::mcu_reset(model::Id component)
{
    if (active()) {
        Writer w(Type::McuReset);
        w.u64(component);
        link_.send(w.payload());
    }
}

mcu::PinLog RemoteSession::mcu_pin_log(model::Id component, const std::string& pin, std::uint64_t from_ns) const
{
    mcu::PinLog result;
    const auto board = boards_.find(component);
    if (board == boards_.end()) {
        return result;
    }
    const auto it = board->second.levels.find(pin);
    if (it == board->second.levels.end() || it->second.empty()) {
        return result;
    }
    // As NativeMcuRunner::pin_log: the level at `from_ns` and the changes after it.
    const auto& log = it->second;
    auto after = std::upper_bound(
        log.begin(), log.end(), from_ns, [](std::uint64_t t, const auto& entry) { return t < entry.first; });
    result.initial = after == log.begin() ? log.front().second : std::prev(after)->second;
    result.changes.assign(after, log.end());
    return result;
}

void RemoteSession::mcu_uart_send(model::Id component, const std::string& uart, const std::string& bytes)
{
    if (active()) {
        Writer w(Type::UartSend);
        w.u64(component);
        w.str(uart);
        w.str(bytes);
        link_.send(w.payload());
    }
}

std::uint64_t RemoteSession::mcu_uart_received(model::Id component, const std::string& uart, std::uint64_t from,
    std::string& bytes, std::vector<std::uint64_t>* times) const
{
    bytes.clear();
    if (times) {
        times->clear();
    }
    const auto board = boards_.find(component);
    if (board == boards_.end()) {
        return 0;
    }
    const auto it = board->second.uarts.find(uart);
    if (it == board->second.uarts.end()) {
        return 0;
    }
    const auto& log = it->second;
    const std::uint64_t first = log.total - log.bytes.size();
    const std::uint64_t begin = std::max(from, first);
    if (begin < log.total) {
        const auto offset = static_cast<std::size_t>(begin - first);
        bytes = log.bytes.substr(offset);
        if (times) {
            times->assign(log.times.begin() + static_cast<std::ptrdiff_t>(offset), log.times.end());
        }
    }
    return log.total;
}

void RemoteSession::receive(const std::string& payload)
{
    Reader r(payload);
    switch (r.type()) {
    case Type::Started: {
        started_ok_ = r.boolean();
        started_error_ = r.str();
        status_ = protocol::read_status(r);
        for (std::size_t n = r.size(4); n > 0; --n) {
            auto name = r.str();
            rings_[name] = std::make_unique<SampleRing>(ring_capacity_);
            vectors_.push_back(std::move(name));
        }
        if (!r.ok()) {
            started_ok_ = false;
            started_error_ = "The simulation engine sent an incomplete answer.";
        }
        started_ = true;
        break;
    }
    case Type::Update:
        apply_update(r);
        break;
    case Type::Stopped:
        stopped_ = true;
        break;
    default:
        break;
    }
}

void RemoteSession::apply_update(Reader& r)
{
    const RunStatus status = protocol::read_status(r);
    std::map<std::string, double> latest;
    for (std::size_t n = r.size(12); n > 0; --n) {
        auto name = r.str();
        latest[name] = r.f64();
    }
    for (std::size_t n = r.size(17); n > 0; --n) {
        const model::Id id = r.u64();
        const bool has = r.boolean();
        const double value = r.f64();
        // The engine's value is the starting one; later ones come from here.
        if (has && !controls_.count(id)) {
            controls_[id] = value;
        }
    }
    for (std::size_t n = r.size(8); n > 0; --n) {
        const std::uint32_t index = r.u32();
        const auto samples = protocol::read_samples(r);
        if (index < vectors_.size()) {
            auto& ring = *rings_[vectors_[index]];
            for (const auto& s : samples) {
                ring.push(s.time, s.value);
            }
        }
    }
    for (std::size_t n = r.size(12); n > 0; --n) {
        const model::Id id = r.u64();
        mcu_diagnostics_.emplace_back(id, r.str());
    }
    for (std::size_t n = r.size(8); n > 0 && r.ok(); --n) {
        auto& board = boards_[r.u64()];
        const bool has = r.boolean();
        const auto mcu = protocol::read_mcu_status(r);
        board.status = has ? std::optional<mcu::McuStatus>(mcu) : std::nullopt;
        board.pins.clear();
        for (std::size_t p = r.size(8); p > 0; --p) {
            auto name = r.str();
            board.pins[name] = protocol::read_pin_state(r);
        }
        for (std::size_t u = r.size(16); u > 0; --u) {
            auto& log = board.uarts[r.str()];
            log.total = r.u64();
            log.bytes += r.str();
            for (std::size_t t = r.size(8); t > 0; --t) {
                log.times.push_back(r.u64());
            }
            if (log.bytes.size() > uart_keep) {
                const std::size_t drop = log.bytes.size() - uart_keep;
                log.bytes.erase(0, drop);
                log.times.erase(log.times.begin(), log.times.begin() + static_cast<std::ptrdiff_t>(drop));
            }
        }
        for (std::size_t p = r.size(10); p > 0; --p) {
            auto& log = board.levels[r.str()];
            const bool reset = r.boolean();
            const bool initial = r.boolean();
            if (reset || log.empty()) {
                log.assign(1, {0, initial});
            }
            for (std::size_t c = r.size(9); c > 0; --c) {
                const std::uint64_t t = r.u64();
                log.emplace_back(t, r.boolean());
            }
            const bool level = r.boolean();
            if (log.back().second != level) {
                log.emplace_back(log.back().first, level);
            }
            if (log.size() > log_keep) {
                log.erase(log.begin(), log.begin() + static_cast<std::ptrdiff_t>(log_keep / 10));
            }
        }
    }
    for (std::size_t n = r.size(12); n > 0; --n) {
        const model::Id id = r.u64();
        texts_[id] = r.str();
    }
    if (r.boolean()) {
        transient_ = protocol::read_transient(r);
    }
    if (!r.ok()) {
        return; // a damaged message changes nothing more
    }
    status_ = status;
    latest_ = std::move(latest);
}

void RemoteSession::lost(const std::string& reason)
{
    lost_ = true;
    lost_reason_ = reason;
    engine_run_ = false;
    if (status_.state == RunState::Running || status_.state == RunState::Paused) {
        status_.state = RunState::Failed;
        status_.message = reason;
    }
}

bool RemoteSession::wait(const std::function<bool()>& done, int timeout_ms)
{
    using clock = std::chrono::steady_clock;
    const auto end = clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!done()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(end - clock::now()).count();
        if (lost_ || left <= 0) {
            return false;
        }
        if (!link_.pump(static_cast<int>(left))) {
            return done();
        }
    }
    return true;
}

} // namespace simnodus::sim
