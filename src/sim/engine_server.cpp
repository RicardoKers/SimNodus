// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "sim/engine_server.hpp"

#include "library/blocks.hpp"
#include "sim/engine_host.hpp"

#include <algorithm>
#include <chrono>
#include <limits>

namespace simnodus::sim {

using protocol::Reader;
using protocol::Type;
using protocol::Writer;

EngineServer::EngineServer(Send send) : send_(std::move(send)) {}

EngineServer::~EngineServer()
{
    session_.stop();
}

void EngineServer::handle(const std::string& payload)
{
    Reader r(payload);
    switch (r.type()) {
    case Type::Start:
        start(r);
        break;
    case Type::Pause:
        session_.pause();
        break;
    case Type::Resume:
        session_.resume();
        break;
    case Type::Stop:
        session_.stop();
        publish(true);
        send_(Writer(Type::Stopped).payload());
        break;
    case Type::Speed:
        session_.set_speed(r.f64());
        break;
    case Type::PauseAt:
        session_.pause_at(r.f64());
        break;
    case Type::Control: {
        const model::Id id = r.u64();
        session_.set_control(id, r.f64());
        break;
    }
    case Type::McuReset:
        session_.mcu_reset(r.u64());
        break;
    case Type::UartSend: {
        const model::Id id = r.u64();
        auto uart = r.str();
        auto bytes = r.str();
        if (r.ok()) {
            session_.mcu_uart_send(id, uart, bytes);
        }
        break;
    }
    case Type::DeviceParameter: {
        const model::Id id = r.u64();
        auto name = r.str();
        auto value = r.str();
        if (r.ok()) {
            session_.set_device_parameter(id, name, value);
        }
        break;
    }
    case Type::LoadMemory: {
        const model::Id id = r.u64();
        const auto bytes = r.str();
        if (r.ok()) {
            session_.load_device_memory(id, {bytes.begin(), bytes.end()});
        }
        break;
    }
    case Type::Quit:
        session_.stop();
        quit_ = true;
        break;
    default:
        break; // unknown messages are ignored
    }
}

void EngineServer::start(Reader& r)
{
    session_.stop();
    RunRequest request;
    protocol::EngineSetup setup;
    std::string error;
    bool ok = protocol::read_start(r, request, setup, error);
    if (ok) {
        if (setup.ngspice) {
            EngineHost::instance().set_location(*setup.ngspice);
        } else if (!setup.application_dir.empty()) {
            EngineHost::instance().set_application_dir(setup.application_dir);
        }
        if (base_dir_ != setup.library_dir || base_.empty()) {
            base_ = {};
            base_.load_directory(setup.library_dir);
            base_dir_ = setup.library_dir;
        }
        library_ = std::make_unique<library::ComponentLibrary>(library::with_blocks(base_, request.schematic));
        request.library = library_.get();
        ok = session_.start(std::move(request), error);
    }
    vectors_ = ok ? session_.acquired_vectors() : std::vector<std::string>{};
    sent_memories_.clear();
    cursors_.assign(vectors_.size(), 0);
    boards_.clear();
    transient_sent_ = false;
    published_end_ = !ok;

    Writer w(Type::Started);
    w.boolean(ok);
    w.str(error);
    protocol::write_status(w, session_.status());
    w.size(vectors_.size());
    for (const auto& v : vectors_) {
        w.str(v);
    }
    send_(w.payload());
}

void EngineServer::publish(bool force)
{
    const RunStatus status = session_.status();
    const bool active = status.state == RunState::Running || status.state == RunState::Paused;
    if (!active && published_end_ && !force) {
        return;
    }
    published_end_ = !active;

    Writer w(Type::Update);
    protocol::write_status(w, status);
    const auto latest = session_.latest_all();
    w.size(latest.size());
    for (const auto& [name, value] : latest) {
        w.str(name);
        w.f64(value);
    }
    const auto& controls = session_.netlist().controls;
    w.size(controls.size());
    for (const auto& c : controls) {
        const auto value = session_.control(c.component);
        w.u64(c.component);
        w.boolean(value.has_value());
        w.f64(value.value_or(0.0));
    }

    // New samples, by index in the Started message's vector list.
    std::vector<std::pair<std::uint32_t, std::vector<Sample>>> fresh;
    for (std::size_t i = 0; i < vectors_.size(); ++i) {
        auto samples = session_.take_new_samples(vectors_[i], cursors_[i]);
        if (!samples.empty()) {
            fresh.emplace_back(static_cast<std::uint32_t>(i), std::move(samples));
        }
    }
    w.size(fresh.size());
    for (const auto& [index, samples] : fresh) {
        w.u32(index);
        protocol::write_samples(w, samples);
    }

    const auto diagnostics = session_.take_mcu_diagnostics();
    w.size(diagnostics.size());
    for (const auto& [id, text] : diagnostics) {
        w.u64(id);
        w.str(text);
    }
    const auto& requests = session_.mcu_requests();
    w.size(requests.size());
    for (const auto& request : requests) {
        const model::Id id = request.component;
        auto& cursor = boards_[id];
        const auto mcu = session_.mcu_status(id);
        w.u64(id);
        w.boolean(mcu.has_value());
        protocol::write_mcu_status(w, mcu.value_or(mcu::McuStatus{}));
        const auto pins = session_.mcu_pins(id);
        w.size(pins.size());
        for (const auto& [name, state] : pins) {
            w.str(name);
            protocol::write_pin_state(w, state);
        }
        w.size(request.options.uarts.size());
        for (const auto& uart : request.options.uarts) {
            std::string bytes;
            std::vector<std::uint64_t> times;
            auto& sent = cursor.uart[uart];
            const std::uint64_t total = session_.mcu_uart_received(id, uart, sent, bytes, &times);
            sent = total;
            w.str(uart);
            w.u64(total);
            w.str(bytes);
            w.size(times.size());
            for (auto t : times) {
                w.u64(t);
            }
        }
        // Pin changes once the board runs: its first log holds the initial levels.
        const bool started = mcu && mcu->state != mcu::McuState::Starting;
        w.size(started ? request.options.pins.size() : 0);
        if (!started) {
            continue;
        }
        for (const auto& pin : request.options.pins) {
            auto& p = cursor.pins[pin];
            const bool reset = !p.sent;
            const auto log = session_.mcu_pin_log(id, pin, reset ? 0 : p.last_ns);
            // The level now: a change logged at the same instant as the last one sent is not in `log`.
            const bool level =
                session_.mcu_pin_log(id, pin, std::numeric_limits<std::uint64_t>::max()).initial;
            w.str(pin);
            w.boolean(reset);
            w.boolean(log.initial);
            w.size(log.changes.size());
            for (const auto& [t, high] : log.changes) {
                w.u64(t);
                w.boolean(high);
            }
            w.boolean(level);
            p.sent = true;
            if (!log.changes.empty()) {
                p.last_ns = log.changes.back().first;
            }
        }
    }

    const auto texts = session_.device_texts();
    w.size(texts.size());
    for (const auto& [id, text] : texts) {
        w.u64(id);
        w.str(text);
    }
    // Memory chips whose contents changed since the last Update (SN-208).
    std::vector<std::pair<model::Id, std::vector<std::uint8_t>>> memories;
    for (const auto& m : session_.mcu_requests()) {
        std::vector<model::Id> ids{m.component}; // the board's flash after esptool (SN-253)
        for (const auto& d : m.options.devices) {
            ids.push_back(d.component);
        }
        for (const model::Id id : ids) {
            auto bytes = session_.device_memory(id);
            if (bytes && sent_memories_[id] != *bytes) {
                sent_memories_[id] = *bytes;
                memories.emplace_back(id, std::move(*bytes));
            }
        }
    }
    w.size(memories.size());
    for (const auto& [id, bytes] : memories) {
        w.u64(id);
        w.str(std::string(bytes.begin(), bytes.end()));
    }

    const bool transient = status.state == RunState::Finished && status.mode == model::SimulationMode::Transient;
    const bool send_transient = transient && !transient_sent_;
    w.boolean(send_transient);
    if (send_transient) {
        protocol::write_transient(w, session_.transient_result());
        transient_sent_ = true;
    }
    send_(w.payload());
}

void EngineServer::serve(const std::function<std::optional<std::string>(int, bool&)>& next, int period_ms)
{
    using clock = std::chrono::steady_clock;
    auto due = clock::now() + std::chrono::milliseconds(period_ms);
    while (!quit_) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(due - clock::now()).count();
        bool closed = false;
        auto message = next(static_cast<int>(std::max<long long>(0, left)), closed);
        if (closed) {
            session_.stop();
            return;
        }
        if (message) {
            handle(*message);
        }
        if (clock::now() >= due) {
            publish();
            due = clock::now() + std::chrono::milliseconds(period_ms);
        }
    }
}

} // namespace simnodus::sim
