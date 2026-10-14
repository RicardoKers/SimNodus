// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Microcontroller engine against the real Renode 1.16.1 (SN-130):
// renode_tests <renode-executable> <mcu-resources-dir> [filter].
// The machine is the owned STM32F103C8 platform without firmware, so these
// cases cover the process lifecycle, time control, GPIO and bus access, not
// firmware behavior.
#include "test_support.hpp"

#include "mcu/renode_engine.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

using namespace simnodus::mcu;
namespace fs = std::filesystem;

namespace {

fs::path renode;
fs::path resources;

constexpr std::uint64_t gpioa_idr = 0x40010808;

// A copy of the support files in a folder whose path contains a space.
fs::path spaced_resources()
{
    static const fs::path folder = [] {
        const fs::path target = fs::temp_directory_path() / "SimNodus tests" / "mcu files";
        fs::create_directories(target);
        fs::copy(resources, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        return target;
    }();
    return folder;
}

MachineOptions options()
{
    MachineOptions o;
    o.renode = renode;
    o.resources = spaced_resources();
    return o;
}

std::unique_ptr<RenodeEngine> start_engine(const MachineOptions& machine = options())
{
    std::string error;
    const auto begin = std::chrono::steady_clock::now();
    auto engine = RenodeEngine::start(machine, error);
    if (!engine) {
        std::printf("  start failed: %s\n", error.c_str());
    } else {
        std::printf("  Renode ready in %.1f s\n",
            std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());
    }
    return engine;
}

void kill_process(long long id)
{
#ifdef _WIN32
    if (HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(id))) {
        TerminateProcess(process, 9);
        CloseHandle(process);
    }
#else
    ::kill(static_cast<pid_t>(id), SIGKILL);
#endif
}

} // namespace

TEST_CASE("an empty STM32 machine starts paused at time zero and stops cleanly")
{
    auto engine = start_engine();
    REQUIRE(engine);
    CHECK(engine->usable());
    CHECK(engine->time_us() == std::uint64_t{0});
    CHECK(engine->time_us() == std::uint64_t{0}); // reading the time does not advance it
    const fs::path folder = engine->run_directory();
    CHECK(fs::exists(engine->log_file()));
    CHECK(engine->stop() == 0);
    CHECK(!fs::exists(folder));
    CHECK(!engine->usable());
}

TEST_CASE("run_for advances virtual time by exactly the requested microseconds")
{
    auto engine = start_engine();
    REQUIRE(engine);
    std::uint64_t expected = 0;
    for (std::uint64_t step : {0ull, 1ull, 999ull, 1000ull, 100000ull, 1000000ull}) {
        REQUIRE(engine->run_for(step));
        expected += step;
        const auto now = engine->time_us();
        REQUIRE(now.has_value());
        CHECK(*now == expected);
    }
    CHECK(engine->time_us() == expected);
    CHECK(engine->stop() == 0);
}

TEST_CASE("a GPIO input set from outside reaches the input data register")
{
    auto engine = start_engine();
    REQUIRE(engine);
    REQUIRE(engine->run_for(10));
    REQUIRE(engine->set_gpio("gpioPortA", 1, true));
    CHECK(engine->time_us() == std::uint64_t{10}); // setting an input does not advance time
    auto idr = engine->read32(gpioa_idr);
    REQUIRE(idr.has_value());
    CHECK((*idr & 0x2u) != 0);
    CHECK(engine->gpio("gpioPortA", 1) == true);
    REQUIRE(engine->set_gpio("gpioPortA", 1, false));
    idr = engine->read32(gpioa_idr);
    REQUIRE(idr.has_value());
    CHECK((*idr & 0x2u) == 0);
    CHECK(engine->stop() == 0);
}

TEST_CASE("GPIO output changes made by the CPU are reported with their virtual time")
{
    // No toolchain is needed: a hand-assembled Thumb loop in flash toggles PA0.
    //   0x08000008  4802  ldr  r0, [pc, #8]   ; r0 = &GPIOA_ODR (literal at 0x08000014)
    //   0x0800000A  2101  movs r1, #1
    //   0x0800000C  6001  str  r1, [r0]
    //   0x0800000E  2100  movs r1, #0
    //   0x08000010  6001  str  r1, [r0]
    //   0x08000012  E7FA  b    0x0800000A
    MachineOptions machine = options();
    machine.after_firmware = {
        "sysbus WriteDoubleWord 0x08000000 0x20005000", // initial stack pointer
        "sysbus WriteDoubleWord 0x08000004 0x08000009", // reset vector (Thumb)
        "sysbus WriteDoubleWord 0x08000008 0x21014802",
        "sysbus WriteDoubleWord 0x0800000C 0x21006001",
        "sysbus WriteDoubleWord 0x08000010 0xE7FA6001",
        "sysbus WriteDoubleWord 0x08000014 0x4001080C",
        "sysbus WriteDoubleWord 0x40010800 0x44444443", // PA0 push-pull output
        "sysbus.cpu VectorTableOffset 0x08000000",
        "sysbus.cpu PC 0x08000008",
    };
    machine.quantum_us = 10;
    auto engine = start_engine(machine);
    REQUIRE(engine);
    REQUIRE(engine->watch_gpio("gpioPortA", 0));
    // A change made from outside while time stands still is not an event (the
    // server reports events only during RunFor); the CPU's changes are.
    REQUIRE(engine->run_for(20));
    const auto events = engine->take_gpio_events();
    std::printf("  %zu events in 20 us", events.size());
    for (std::size_t i = 0; i < events.size() && i < 4; ++i) {
        std::printf(", t=%llu %d", static_cast<unsigned long long>(events[i].time_us), events[i].state ? 1 : 0);
    }
    std::printf("\n");
    REQUIRE(events.size() >= 4);
    bool alternating = true;
    bool ordered = true;
    for (std::size_t i = 0; i < events.size(); ++i) {
        alternating = alternating && events[i].state == (i % 2 == 0);
        ordered = ordered && (i == 0 || events[i].time_us >= events[i - 1].time_us);
        CHECK(events[i].time_us <= 20);
        CHECK(events[i].port == "gpioPortA" && events[i].pin == 0);
    }
    CHECK(alternating);
    CHECK(ordered);
    CHECK(engine->time_us() == std::uint64_t{20});
    CHECK(engine->stop() == 0);

    // A second run starts with its own callback table.
    auto second = start_engine();
    REQUIRE(second);
    CHECK(second->watch_gpio("gpioPortA", 0));
    CHECK(second->stop() == 0);
}

TEST_CASE("unknown peripherals fail the command but keep the connection")
{
    auto engine = start_engine();
    REQUIRE(engine);
    CHECK(!engine->watch_gpio("gpioPortZ", 0));
    CHECK(!engine->error().empty());
    std::printf("  %s\n", engine->error().c_str());
    CHECK(engine->usable());
    CHECK(engine->run_for(5));
    CHECK(engine->time_us() == std::uint64_t{5});
    CHECK(engine->stop() == 0);
}

TEST_CASE("missing files and a broken platform are reported, not hung")
{
    std::string error;
    auto o = options();
    o.firmware = fs::temp_directory_path() / "SimNodus tests" / "missing.elf";
    CHECK(!RenodeEngine::start(o, error));
    CHECK(error.find("firmware") != std::string::npos);

    o = options();
    o.renode = renode.parent_path() / "no-renode-here";
    error.clear();
    CHECK(!RenodeEngine::start(o, error));
    CHECK(error.find("Renode was not found") != std::string::npos);

    const fs::path broken = fs::temp_directory_path() / "SimNodus tests" / "broken platform";
    fs::create_directories(broken);
    fs::copy_file(resources / "LoopbackControl.cs", broken / "LoopbackControl.cs", fs::copy_options::overwrite_existing);
    std::ofstream(broken / "stm32f103c8.repl") << "this is not a platform description\n";
    o = options();
    o.resources = broken;
    error.clear();
    const auto begin = std::chrono::steady_clock::now();
    CHECK(!RenodeEngine::start(o, error));
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::printf("  broken platform reported after %.1f s\n", seconds);
    CHECK(error.find("could not set up") != std::string::npos);
    CHECK(seconds < 30.0);
}

TEST_CASE("a backend that dies fails the next call instead of hanging")
{
    auto engine = start_engine();
    REQUIRE(engine);
    REQUIRE(engine->run_for(10));
    kill_process(engine->process_id());
    const auto begin = std::chrono::steady_clock::now();
    CHECK(!engine->run_for(1000));
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    CHECK(seconds < 12.0);
    CHECK(!engine->usable());
    std::printf("  after the kill: %s\n", engine->error().c_str());
    CHECK(engine->stop() != 0);
}

TEST_CASE("find_renode prefers SIMNODUS_RENODE")
{
#ifdef _WIN32
    _putenv_s("SIMNODUS_RENODE", renode.string().c_str());
#else
    setenv("SIMNODUS_RENODE", renode.string().c_str(), 1);
#endif
    const auto found = find_renode(fs::temp_directory_path());
    REQUIRE(found.has_value());
    CHECK(fs::equivalent(*found, renode));
    CHECK(find_mcu_resources(fs::temp_directory_path()).has_value()); // source-tree fallback
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::printf("usage: renode_tests <renode-executable> <mcu-resources-dir> [filter]\n");
        return 2;
    }
    renode = argv[1];
    resources = argv[2];
    return simnodus::test::run_all(argc > 3 ? argv[3] : "");
}
