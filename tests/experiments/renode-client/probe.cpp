// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include <windows.h>
#undef NO_ERROR
extern "C" {
#include "renode_api.h"
#include "transport.h"
}
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
int consume(renode_error_t* error)
{
    if (!error) { return ERR_NO_ERROR; }
    const int code = error->code;
    std::cerr << "Backend diagnostic " << code << ": " << (error->message ? error->message : "(none)") << '\n';
    renode_free_error(error);
    return code;
}
void success(renode_error_t* error)
{
    require(consume(error) == ERR_NO_ERROR, "Unexpected backend error");
}
struct Connection {
    renode_t* value = nullptr;
    ~Connection() { if (value) { consume(renode_disconnect(&value)); } }
};
uint64_t time_us(Connection& connection)
{
    uint64_t value = 0;
    success(renode_get_current_time(connection.value, TU_MICROSECONDS, &value));
    return value;
}
void disconnect(Connection& connection)
{
    success(renode_disconnect(&connection.value));
    require(connection.value == nullptr, "Disconnect did not clear the handle");
}
void real(const char* port)
{
    uint64_t previous_end = 0;
    std::cout << "{\"transfer_limit\":" << SN_TRANSFER_LIMIT << ",\"cycles\":[";
    for (int cycle = 0; cycle < 3; ++cycle) {
        Connection connection;
        success(renode_connect(port, &connection.value));
        const auto initial = time_us(connection);
        if (cycle) { require(initial == previous_end, "Reconnect changed emulation time"); }
        renode_machine_t* machine = nullptr;
        success(renode_get_machine(connection.value, "sn019", &machine));
        std::free(machine); machine = nullptr;
        require(consume(renode_get_machine(connection.value, "missing", &machine)) == ERR_COMMAND_FAILED,
            "Missing machine was not a recoverable command error");
        require(machine == nullptr && time_us(connection) == initial, "Lookup failure corrupted state");
        require(consume(renode_run_for(connection.value, static_cast<renode_time_unit_t>(0), 1)) == ERR_FATAL,
            "Invalid unit was accepted");
        require(consume(renode_run_for(connection.value, TU_SECONDS, UINT64_MAX)) == ERR_FATAL,
            "Overflowing duration was accepted");
        require(time_us(connection) == initial, "Invalid command advanced time");
        if (cycle) { std::cout << ','; }
        std::cout << "{\"initial_us\":" << initial << ",\"steps\":[";
        const std::vector<std::pair<renode_time_unit_t, uint64_t>> steps = {
            {TU_MICROSECONDS, 0}, {TU_MICROSECONDS, 1}, {TU_MICROSECONDS, 999},
            {TU_MICROSECONDS, 1000}, {TU_MILLISECONDS, 1}, {TU_SECONDS, 1}};
        bool first = true;
        for (const auto& [unit, value] : steps) {
            const auto before = time_us(connection);
            require(time_us(connection) == before, "Time query advanced emulation");
            success(renode_run_for(connection.value, unit, value));
            const auto after = time_us(connection);
            const uint64_t requested = value * static_cast<uint64_t>(unit);
            require(after >= before && after - before == requested, "Actual time differs from requested interval");
            if (!first) { std::cout << ','; }
            first = false;
            std::cout << "{\"requested_us\":" << requested << ",\"before_us\":" << before << ",\"after_us\":" << after << '}';
        }
        previous_end = time_us(connection);
        disconnect(connection);
        std::cout << "]}";
    }
    std::cout << "]}\n";
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) { std::cerr << "Usage: probe <port> <real|connect|query|run|stress>\n"; return 2; }
    try {
        const std::string mode = argv[2];
        if (mode == "real") { real(argv[1]); return 0; }
        if (mode == "stress") {
            DWORD before = 0, after = 0;
            { Connection warmup; require(consume(renode_connect(argv[1], &warmup.value)) == ERR_FATAL, "Warmup must reject handshake"); }
            require(GetProcessHandleCount(GetCurrentProcess(), &before), "Cannot count handles");
            for (int i = 0; i < 20; ++i) {
                Connection connection;
                require(consume(renode_connect(argv[1], &connection.value)) == ERR_FATAL && !connection.value,
                    "Failed handshake retained a connection");
            }
            require(GetProcessHandleCount(GetCurrentProcess(), &after), "Cannot count handles");
            require(after <= before + 2, "Repeated failure grew the handle count");
            std::cout << "{\"attempts\":21,\"handles_before\":" << before << ",\"handles_after\":" << after << "}\n";
            return 0;
        }
        require(mode == "connect" || mode == "query" || mode == "run", "Unknown mode");
        Connection connection;
        const auto started = std::chrono::steady_clock::now();
        const int connect_code = consume(renode_connect(argv[1], &connection.value));
        int operation_code = ERR_NO_ERROR, reuse_code = ERR_NO_ERROR;
        uint64_t value = 0;
        if (connect_code == ERR_NO_ERROR && mode != "connect") {
            operation_code = consume(mode == "run" ? renode_run_for(connection.value, TU_MICROSECONDS, 1)
                : renode_get_current_time(connection.value, TU_MICROSECONDS, &value));
            reuse_code = consume(renode_get_current_time(connection.value, TU_MICROSECONDS, &value));
        }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        if (connection.value) { disconnect(connection); }
        std::cout << "{\"connect_code\":" << connect_code << ",\"operation_code\":" << operation_code
                  << ",\"reuse_code\":" << reuse_code << ",\"value_us\":" << value
                  << ",\"wall_seconds\":" << elapsed << ",\"handle_cleared\":" << (connection.value ? "false" : "true") << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SN-019 probe failed: " << error.what() << '\n';
        return 1;
    }
}
