// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The editor/engine process boundary (SN-151): a RemoteSession talks to an
// EngineServer through an in-memory link, with the server on its own thread
// as in the engine process. The real process is exercised by the GUI tests.
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "engine_fixture.hpp"
#include "io/project_file.hpp"
#include "sim/engine_protocol.hpp"
#include "sim/engine_server.hpp"
#include "sim/remote_session.hpp"

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

using namespace simnodus;
using namespace std::chrono_literals;

namespace {

library::ComponentLibrary& parts()
{
    static library::ComponentLibrary library = [] {
        library::ComponentLibrary l;
        l.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
        return l;
    }();
    return library;
}

// The engine finds the library; ngspice is the one the fixture loaded.
sim::protocol::EngineSetup test_setup()
{
    sim::protocol::EngineSetup setup;
    setup.library_dir = SIMNODUS_TEST_LIBRARY_DIR;
    return setup;
}

// Two queues and an EngineServer thread; every payload goes through a frame and
// back, as over the pipes.
class Loopback : public sim::EngineLink {
public:
    ~Loopback() override { kill(); }

    bool open(std::string&) override
    {
        if (server_.joinable()) {
            return true;
        }
        gone_ = false;
        quit_ = false;
        opened_++;
        server_ = std::thread([this] {
            sim::EngineServer server([this](const std::string& payload) {
                std::lock_guard lock(mutex_);
                reply_frames_.append(sim::protocol::frame(payload));
                replied_.notify_all();
            });
            server.serve([this](int timeout_ms, bool& closed) -> std::optional<std::string> {
                std::unique_lock lock(mutex_);
                commanded_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return quit_ || !commands_.empty(); });
                closed = quit_;
                if (commands_.empty()) {
                    return std::nullopt;
                }
                auto payload = std::move(commands_.front());
                commands_.pop_front();
                return payload;
            });
        });
        return true;
    }

    bool send(const std::string& payload) override
    {
        std::lock_guard lock(mutex_);
        if (gone_) {
            return false;
        }
        ++sent_;
        commands_.push_back(payload);
        commanded_.notify_all();
        return true;
    }

    bool pump(int timeout_ms) override
    {
        std::string bytes;
        {
            std::unique_lock lock(mutex_);
            replied_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return gone_ || !reply_frames_.empty(); });
            bytes.swap(reply_frames_);
        }
        frames_.append(bytes.data(), bytes.size());
        while (auto payload = frames_.next()) {
            if (on_message) {
                on_message(*payload);
            }
        }
        return !gone_;
    }

    void kill() override
    {
        {
            std::lock_guard lock(mutex_);
            quit_ = true;
            gone_ = true;
            commands_.clear();
            commanded_.notify_all();
        }
        if (server_.joinable()) {
            server_.join();
        }
        std::lock_guard lock(mutex_);
        reply_frames_.clear();
        frames_ = {};
    }

    // The engine process dies.
    void crash()
    {
        kill();
        if (on_lost) {
            on_lost("The simulation engine stopped unexpectedly.");
        }
    }

    int sent() const { return sent_; }
    int opened() const { return opened_; }

private:
    std::thread server_;
    std::mutex mutex_;
    std::condition_variable commanded_;
    std::condition_variable replied_;
    std::deque<std::string> commands_;
    std::string reply_frames_;
    sim::protocol::FrameReader frames_;
    bool quit_ = false;
    bool gone_ = false;
    int sent_ = 0;
    int opened_ = 0;
};

// Pumps until `done` (or 60 s).
template <typename Done> bool pump_until(Loopback& link, Done done)
{
    const auto end = std::chrono::steady_clock::now() + 60s;
    while (!done()) {
        if (std::chrono::steady_clock::now() > end) {
            return false;
        }
        link.pump(10);
    }
    return true;
}

sim::RunRequest rc_request(model::SimulationMode mode, model::Id* button = nullptr)
{
    test::CircuitBuilder b(parts());
    b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
    const model::Id sw = b.add("button_no", {{"1", "VCC"}, {"2", "SW"}});
    b.add("resistor", {{"1", "SW"}, {"2", "CAP"}}, {{"R", "1k"}});
    b.add("capacitor", {{"1", "CAP"}, {"2", "0"}}, {{"C", "10u"}}); // tau = 10 ms
    b.add("resistor", {{"1", "CAP"}, {"2", "0"}}, {{"R", "100k"}});
    auto settings = b.schematic.settings();
    settings.mode = mode;
    settings.speed = 0.0;
    settings.segment = 0.25;
    settings.max_step = 1e-4;
    settings.transient_stop = 0.05;
    settings.transient_max_step = 1e-4;
    b.schematic.set_settings(settings);
    if (button) {
        *button = sw;
    }
    sim::RunRequest r;
    r.schematic = b.schematic;
    r.library = &parts();
    r.vectors = {"cap", "sw"};
    r.ring_capacity = 50000;
    return r;
}

} // namespace

TEST_CASE("remote: frames survive splitting and a bad length ends the stream")
{
    sim::protocol::Writer w(sim::protocol::Type::Speed);
    w.f64(0.5);
    const auto bytes = sim::protocol::frame(w.payload()) + sim::protocol::frame(sim::protocol::Writer(sim::protocol::Type::Stop).payload());
    sim::protocol::FrameReader frames;
    for (char c : bytes) {
        frames.append(&c, 1);
    }
    auto first = frames.next();
    REQUIRE(first.has_value());
    sim::protocol::Reader r(*first);
    CHECK(r.type() == sim::protocol::Type::Speed);
    CHECK(r.f64() == 0.5 && r.ok());
    CHECK(r.u32() == 0 && !r.ok()); // reading past the end is detected
    auto second = frames.next();
    REQUIRE(second.has_value());
    CHECK(sim::protocol::Reader(*second).type() == sim::protocol::Type::Stop);
    CHECK(!frames.next().has_value());

    sim::protocol::FrameReader bad;
    const std::uint32_t huge = 0xFFFFFFFFu;
    bad.append(reinterpret_cast<const char*>(&huge), sizeof huge);
    CHECK(!bad.next().has_value() && bad.invalid());
}

TEST_CASE("remote: a start message carries the whole run request")
{
    model::Id button = model::no_id;
    auto request = rc_request(model::SimulationMode::Interactive, &button);
    request.current_probes = {{button, "2"}};
    sim::McuRequest board;
    board.component = 42;
    board.uart_pins["usart1"] = {"PA9", "PA10"};
    board.options.firmware = std::filesystem::path(u8"C:/Usuários/aluno/blink.elf");
    board.options.pins = {"PA0", "PC13"};
    board.options.uart_rx_byte_links = {"usart2"};
    board.options.gdb_port = 3333;
    request.mcus.push_back(board);
    request.debugging = true;
    sim::protocol::EngineSetup setup;
    setup.library_dir = "lib";
    setup.application_dir = "app";
    const auto payload = sim::protocol::start_message(request, setup);
    sim::protocol::Reader r(payload);
    REQUIRE(r.type() == sim::protocol::Type::Start);
    sim::RunRequest read;
    sim::protocol::EngineSetup read_setup;
    std::string error;
    REQUIRE(sim::protocol::read_start(r, read, read_setup, error));
    CHECK(read_setup.library_dir == "lib" && read_setup.application_dir == "app");
    CHECK(io::to_json_text(read.schematic) == io::to_json_text(request.schematic));
    CHECK(read.vectors == request.vectors);
    CHECK(read.current_probes == request.current_probes);
    CHECK(read.ring_capacity == 50000 && read.debugging);
    REQUIRE(read.mcus.size() == 1);
    CHECK(read.mcus[0].component == 42);
    CHECK(read.mcus[0].uart_pins.at("usart1").second == "PA10");
    CHECK(read.mcus[0].options.firmware == board.options.firmware);
    CHECK(read.mcus[0].options.pins == board.options.pins);
    CHECK(read.mcus[0].options.uart_rx_byte_links.count("usart2") == 1);
    CHECK(read.mcus[0].options.gdb_port == 3333);

    // A message of another version is refused.
    std::string other = payload;
    other[1] = static_cast<char>(other[1] + 1);
    sim::protocol::Reader r2(other);
    r2.type();
    sim::RunRequest ignored;
    CHECK(!sim::protocol::read_start(r2, ignored, read_setup, error) && !error.empty());
}

TEST_CASE("remote: circuit errors are reported without the engine")
{
    test::CircuitBuilder b(parts());
    b.add("resistor", {{"1", "A"}, {"2", "B"}}, {{"R", "1k"}}); // no ground
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &parts();
    Loopback link;
    sim::RemoteSession session(link, test_setup());
    std::string error;
    CHECK(!session.start(request, error));
    CHECK(!error.empty());
    CHECK(!session.diagnostics().empty());
    CHECK(link.opened() == 0 && link.sent() == 0);
}

TEST_CASE("remote: an interactive run is mirrored and controlled")
{
    REQUIRE(test::engine() != nullptr);
    model::Id button = model::no_id;
    Loopback link;
    sim::RemoteSession session(link, test_setup());
    std::string error;
    REQUIRE(session.start(rc_request(model::SimulationMode::Interactive, &button), error));
    CHECK(session.active());
    CHECK(!session.netlist().controls.empty());
    REQUIRE(pump_until(link, [&] { return session.status().sim_time > 0.1; }));
    CHECK(session.control(button).value_or(-1) == 0.0);
    CHECK(session.latest("cap").value_or(-1) < 0.01);

    session.set_control(button, 1.0);
    CHECK(session.control(button).value_or(-1) == 1.0); // at once, before the engine answers
    const double pressed = session.status().sim_time;
    REQUIRE(pump_until(link, [&] { return session.status().sim_time > pressed + 0.1; }));
    // 5 V into 1k / 100k: about 4.95 V after ten time constants.
    CHECK(std::abs(session.latest("CAP").value_or(-1) - 5.0 * 100.0 / 101.0) < 0.02);
    const auto samples = session.samples_since("cap", pressed);
    REQUIRE(samples.size() > 100);
    for (std::size_t i = 1; i < samples.size(); ++i) {
        CHECK(samples[i].time >= samples[i - 1].time);
    }
    CHECK(samples.back().value > 4.9);
    bool initial = true;
    const auto edges = session.transitions_since("sw", 0.0, 2.5, initial);
    CHECK(!initial && edges.size() == 1);

    session.pause();
    REQUIRE(pump_until(link, [&] { return session.status().state == sim::RunState::Paused; }));
    // Pause at a time (SN-215): at the first point at or after it. The mirrored
    // time is exact while paused, so the target is still ahead of the engine.
    const double target = session.status().sim_time + 0.05;
    session.pause_at(target);
    session.resume();
    REQUIRE(pump_until(link, [&] {
        return session.status().state == sim::RunState::Paused && session.status().sim_time >= target;
    }));
    CHECK(session.status().sim_time < target + 0.01);
    session.resume();
    REQUIRE(pump_until(link, [&] { return session.status().state == sim::RunState::Running; }));
    session.stop();
    CHECK(session.status().state == sim::RunState::Stopped);
    CHECK(!session.active());
    CHECK(!session.samples("cap").empty()); // the last run stays readable
}

TEST_CASE("remote: a transient result arrives complete")
{
    REQUIRE(test::engine() != nullptr);
    Loopback link;
    sim::RemoteSession session(link, test_setup());
    std::string error;
    REQUIRE(session.start(rc_request(model::SimulationMode::Transient), error));
    REQUIRE(pump_until(link, [&] { return session.status().state == sim::RunState::Finished; }));
    const auto result = session.transient_result();
    REQUIRE(!result.time.empty());
    CHECK(std::abs(result.time.back() - 0.05) < 1e-6);
    REQUIRE(result.vectors.count("cap") == 1);
    CHECK(result.vectors.at("cap").size() == result.time.size());
    session.stop();
}

TEST_CASE("remote: a lost engine fails the run and the next run starts a new one")
{
    REQUIRE(test::engine() != nullptr);
    Loopback link;
    sim::RemoteSession session(link, test_setup());
    std::string error;
    REQUIRE(session.start(rc_request(model::SimulationMode::Interactive), error));
    REQUIRE(pump_until(link, [&] { return session.status().sim_time > 0.05; }));
    link.crash();
    CHECK(session.status().state == sim::RunState::Failed);
    CHECK(session.status().message.find("stopped unexpectedly") != std::string::npos);
    CHECK(!session.active());
    session.stop(); // nothing to stop: returns at once

    REQUIRE(session.start(rc_request(model::SimulationMode::Interactive), error));
    CHECK(link.opened() == 2);
    REQUIRE(pump_until(link, [&] { return session.status().sim_time > 0.05; }));
    session.stop();
    CHECK(session.status().state == sim::RunState::Stopped);
}

TEST_CASE("remote: a Blue Pill's pins, serial output and status are mirrored")
{
    REQUIRE(test::engine() != nullptr);
    const auto examples = std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR);
    const auto loaded = io::load_project(examples / "09_Blue_Pill_serial_terminal.snx");
    REQUIRE(loaded.schematic.has_value());
    model::Id board = model::no_id;
    for (const auto& c : loaded.schematic->components()) {
        board = c.type == "bluepill" ? c.id : board;
    }
    REQUIRE(board != model::no_id);
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &parts();
    sim::McuEnvironment environment;
    environment.project_dir = examples;
    std::string error;
    REQUIRE(sim::build_mcu_requests(request.schematic, parts(), environment, request.mcus, error));

    Loopback link;
    sim::RemoteSession session(link, test_setup());
    REQUIRE(session.start(request, error));
    // "tick N" every second of simulated time.
    std::string text;
    REQUIRE(pump_until(link, [&] {
        session.mcu_uart_received(board, "usart1", 0, text);
        return text.find("tick 1") != std::string::npos || !session.active();
    }));
    const auto status = session.mcu_status(board);
    REQUIRE(status.has_value());
    CHECK(status->state == mcu::McuState::Running);
    CHECK(status->time_ns > 1000000000u);
    const auto pins = session.mcu_pins(board);
    REQUIRE(pins.count("PA9") == 1);
    CHECK(pins.at("PA9").mode == mcu::PinMode::Alternate);
    // The TX pin's frames are in its log: many changes, levels alternating.
    const auto log = session.mcu_pin_log(board, "PA9", 0);
    CHECK(log.changes.size() > 20);
    bool level = log.initial;
    for (const auto& [t, high] : log.changes) {
        CHECK(high != level);
        level = high;
    }
    // Bytes received from a position, as from the board itself.
    std::string tail;
    std::vector<std::uint64_t> times;
    const auto total = session.mcu_uart_received(board, "usart1", 3, tail, &times);
    CHECK(total == text.size() || total > text.size());
    CHECK(tail.size() == times.size() && tail.substr(0, 2) == text.substr(3, 2));
    session.stop();
}
