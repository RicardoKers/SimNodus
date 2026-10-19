// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The editor side of the process boundary (SN-151, ADR 0101): a Session whose
// run happens in the engine process. Commands go out as messages; the
// engine's Updates are mirrored here (status, latest values, sample rings,
// boards, UART bytes, pin changes, transient result), so the canvas and the
// Signal Analyzer read local data at their own rate. If the engine process
// dies, the run fails with a message and the editor keeps going.
#pragma once

#include "sim/engine_protocol.hpp"
#include "sim/simulation_session.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::sim {

// The editor's end of the connection to an engine process.
class EngineLink {
public:
    virtual ~EngineLink() = default;
    // Makes sure an engine runs (starts one if needed); false with a reason.
    virtual bool open(std::string& error) = 0;
    // Sends one payload; false if the engine is gone.
    virtual bool send(const std::string& payload) = 0;
    // Waits up to `timeout_ms` for data and delivers the complete messages to
    // the receiver; false if the engine is gone.
    virtual bool pump(int timeout_ms) = 0;
    // Ends the engine process at once (it did not answer).
    virtual void kill() = 0;

    // Set by RemoteSession: called for every message and when the engine is lost.
    std::function<void(const std::string& payload)> on_message;
    std::function<void(const std::string& reason)> on_lost;
};

class RemoteSession : public Session {
public:
    RemoteSession(EngineLink& link, protocol::EngineSetup setup);
    ~RemoteSession() override;
    RemoteSession(const RemoteSession&) = delete;
    RemoteSession& operator=(const RemoteSession&) = delete;

    bool start(RunRequest request, std::string& error) override;
    void pause() override;
    void resume() override;
    void stop() override;
    bool active() const override;

    void set_speed(double speed) override;
    void set_control(model::Id component, double value) override;
    std::optional<double> control(model::Id component) const override;

    RunStatus status() const override { return status_; }
    const std::vector<netlist::Diagnostic>& diagnostics() const override { return diagnostics_; }
    const netlist::Netlist& netlist() const override { return netlist_; }

    std::vector<Sample> samples(const std::string& vector) const override;
    std::vector<Sample> samples_since(const std::string& vector, double from, double bucket = 0.0) const override;
    std::vector<std::pair<double, bool>> transitions_since(const std::string& vector, double from, double threshold,
        bool& initial) const override;
    std::optional<double> latest(const std::string& vector) const override;
    std::map<std::string, double> latest_all() const override { return latest_; }
    TransientResult transient_result() const override { return transient_; }

    std::optional<mcu::McuStatus> mcu_status(model::Id component) const override;
    std::vector<std::pair<model::Id, std::string>> take_mcu_diagnostics() override;
    std::map<std::string, mcu::PinState> mcu_pins(model::Id component) const override;
    void mcu_reset(model::Id component) override;
    mcu::PinLog mcu_pin_log(model::Id component, const std::string& pin, std::uint64_t from_ns) const override;
    void mcu_uart_send(model::Id component, const std::string& uart, const std::string& bytes) override;
    std::uint64_t mcu_uart_received(model::Id component, const std::string& uart, std::uint64_t from,
        std::string& bytes, std::vector<std::uint64_t>* times = nullptr) const override;

    // How long start() and stop() wait for the engine's answer.
    void set_timeouts(int start_ms, int stop_ms)
    {
        start_timeout_ms_ = start_ms;
        stop_timeout_ms_ = stop_ms;
    }

private:
    void receive(const std::string& payload);
    void apply_update(protocol::Reader& r);
    void lost(const std::string& reason);
    // Pumps the link until `done` or the timeout; false on a timeout or a lost engine.
    bool wait(const std::function<bool()>& done, int timeout_ms);

    struct UartLog {
        std::string bytes;
        std::vector<std::uint64_t> times;
        std::uint64_t total = 0;
    };
    struct Board {
        std::optional<mcu::McuStatus> status;
        std::map<std::string, mcu::PinState> pins;
        std::map<std::string, UartLog> uarts;
        std::map<std::string, std::vector<std::pair<std::uint64_t, bool>>> levels; // front: initial level
    };

    EngineLink& link_;
    protocol::EngineSetup setup_;
    int start_timeout_ms_ = 30000;
    int stop_timeout_ms_ = 5000;

    netlist::Netlist netlist_;
    std::vector<netlist::Diagnostic> diagnostics_;
    RunStatus status_;
    std::map<std::string, double> latest_;
    std::map<model::Id, double> controls_;
    std::vector<std::string> vectors_;
    std::map<std::string, std::unique_ptr<SampleRing>> rings_;
    std::size_t ring_capacity_ = 0;
    TransientResult transient_;
    std::map<model::Id, Board> boards_;
    std::vector<std::pair<model::Id, std::string>> mcu_diagnostics_;

    bool engine_run_ = false; // a run was started in the engine and not stopped
    // Replies being waited for.
    bool started_ = false;
    bool started_ok_ = false;
    std::string started_error_;
    bool stopped_ = false;
    bool lost_ = false;
    std::string lost_reason_;
};

} // namespace simnodus::sim
