// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/fixture_session.hpp"
#include <memory>
#include <charconv>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    using namespace simnodus;
    if((argc != 2 && argc != 4 && argc != 6 && argc != 8 && argc != 12) || std::string(argv[1]) != "--cooperative-fixture") return 2;
    std::unique_ptr<renode::ControlChannel> channel;
    if(argc >= 4) {
        std::uintptr_t handle = 0;
        const std::string value(argv[3]);
        const auto [end, error] = std::from_chars(value.data(), value.data()+value.size(), handle);
        if(std::string(argv[2]) != "--renode-stdin" || error != std::errc{} || end != value.data()+value.size()) return 2;
        channel = std::make_unique<renode::ControlChannel>(handle);
        if(!channel->valid()) return 2;
    }
    std::unique_ptr<ngspice::WorkerChannel> analog_channel;
    if(argc >= 6) {
        std::uintptr_t handle = 0;
        const std::string value(argv[5]);
        const auto [end, error] = std::from_chars(value.data(), value.data()+value.size(), handle);
        if(std::string(argv[4]) != "--analog-stdin" || error != std::errc{} || end != value.data()+value.size()) return 2;
        analog_channel = std::make_unique<ngspice::WorkerChannel>(handle);
        if(!analog_channel->valid()) return 2;
    }
    std::unique_ptr<ngspice::WorkerReader> worker_reader;
    if(argc >= 8) {
        std::uintptr_t handle = 0;
        const std::string value(argv[7]);
        const auto [end, error] = std::from_chars(value.data(), value.data()+value.size(), handle);
        if(std::string(argv[6]) != "--analog-stdout" || error != std::errc{} || end != value.data()+value.size()) return 2;
        worker_reader = std::make_unique<ngspice::WorkerReader>(handle);
        if(!worker_reader->valid()) return 2;
    }
    std::filesystem::path adc_executable;
    unsigned adc_port = 0;
    if(argc == 12) {
        const std::string path(argv[9]), port(argv[11]);
        const auto [end, error] = std::from_chars(port.data(), port.data()+port.size(), adc_port);
        if(std::string(argv[8]) != "--adc-helper" || std::string(argv[10]) != "--adc-port"
            || error != std::errc{} || end != port.data()+port.size() || adc_port == 0 || adc_port > 65535) return 2;
        adc_executable = std::filesystem::path(std::u8string(path.begin(), path.end()));
        if(!adc_executable.is_absolute()) return 2;
    }
    FixtureSession session(channel.get(), analog_channel.get(), worker_reader.get(), std::move(adc_executable), adc_port);
    session.diagnostics(std::cout);
    std::string line;
    while(std::getline(std::cin, line)) {
        const auto result = session.command(line);
        if(result == FixtureSession::CommandResult::exit_success) return 0;
        if(result == FixtureSession::CommandResult::exit_failure) return 1;
        session.diagnostics(std::cout);
    }
    return 1; // Uncoordinated host loss is never success.
}
