// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The engine side of the process boundary (SN-151, ADR 0101): runs a
// SimulationSession for the editor's messages and sends the run's new state
// back (status, latest values, new samples, boards, UART bytes, pin changes,
// the transient result). The editor's RemoteSession mirrors it.
#pragma once

#include "library/component_library.hpp"
#include "sim/engine_protocol.hpp"
#include "sim/simulation_session.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::sim {

class EngineServer {
public:
    using Send = std::function<void(const std::string& payload)>;
    explicit EngineServer(Send send);
    ~EngineServer();
    EngineServer(const EngineServer&) = delete;
    EngineServer& operator=(const EngineServer&) = delete;

    // Handles one message of the editor.
    void handle(const std::string& payload);
    // Sends an Update while a run is active, and once more when it ends.
    void publish(bool force = false);
    bool quit_requested() const { return quit_; }

    // Reads messages with `next` (it waits at most the given milliseconds and
    // returns nullopt on a timeout, or sets `closed` when the editor is gone)
    // and publishes about every `period_ms`, until Quit or a closed input.
    void serve(const std::function<std::optional<std::string>(int timeout_ms, bool& closed)>& next, int period_ms = 20);

private:
    void start(protocol::Reader& r);

    struct PinCursor {
        bool sent = false;
        std::uint64_t last_ns = 0;
    };
    struct BoardCursor {
        std::map<std::string, std::uint64_t> uart; // bytes sent so far
        std::map<std::string, PinCursor> pins;
    };

    Send send_;
    SimulationSession session_;
    library::ComponentLibrary base_;
    std::filesystem::path base_dir_;
    std::unique_ptr<library::ComponentLibrary> library_; // of the current run
    std::vector<std::string> vectors_;
    std::vector<std::size_t> cursors_; // samples sent per vector
    std::map<model::Id, BoardCursor> boards_;
    bool transient_sent_ = false;
    bool published_end_ = true; // the run's last state was sent
    bool quit_ = false;
};

} // namespace simnodus::sim
