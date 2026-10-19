// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// simnodus-engine (SN-151, ADR 0101): the simulation of the desktop
// application in its own process, so that a crash of ngspice or of the
// emulator cannot close the editor. It reads the editor's messages from
// standard input and writes its own to standard output (framed, see
// sim/engine_protocol.hpp). Whatever the engines print goes to standard error.
// It ends on Quit or when standard input closes (the editor is gone).
#include "sim/engine_protocol.hpp"
#include "sim/engine_server.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define read _read
#define write _write
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace {

bool write_all(int fd, const std::string& data)
{
    std::size_t done = 0;
    while (done < data.size()) {
        const auto chunk = static_cast<unsigned>(std::min<std::size_t>(data.size() - done, 1u << 20));
        const auto n = write(fd, data.data() + done, chunk);
        if (n <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(n);
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--version") {
        std::printf("simnodus-engine protocol %u\n", simnodus::sim::protocol::version);
        return 0;
    }
#ifdef _WIN32
    _setmode(fileno(stdin), _O_BINARY);
    _setmode(fileno(stdout), _O_BINARY);
#endif
    // The protocol gets its own handle of standard output; descriptor 1 (printf,
    // ngspice) then writes to standard error instead.
    std::fflush(stdout);
    const int out = dup(fileno(stdout));
    dup2(fileno(stderr), fileno(stdout));

    std::mutex mutex;
    std::condition_variable arrived;
    std::deque<std::string> inbox;
    bool closed = false;
    bool broken = false; // the editor no longer reads

    std::thread reader([&] {
        simnodus::sim::protocol::FrameReader frames;
        char buffer[1 << 16];
        for (;;) {
            const auto n = read(fileno(stdin), buffer, sizeof buffer);
            if (n <= 0) {
                break;
            }
            frames.append(buffer, static_cast<std::size_t>(n));
            std::lock_guard lock(mutex);
            while (auto payload = frames.next()) {
                inbox.push_back(std::move(*payload));
            }
            arrived.notify_one();
            if (frames.invalid()) {
                break;
            }
        }
        std::lock_guard lock(mutex);
        closed = true;
        arrived.notify_one();
    });
    reader.detach(); // blocked in read() until the editor closes the pipe

    simnodus::sim::EngineServer server([&](const std::string& payload) {
        if (!broken && !write_all(out, simnodus::sim::protocol::frame(payload))) {
            broken = true;
        }
    });
    server.serve([&](int timeout_ms, bool& gone) -> std::optional<std::string> {
        std::unique_lock lock(mutex);
        arrived.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] { return closed || !inbox.empty(); });
        if (!inbox.empty()) {
            auto payload = std::move(inbox.front());
            inbox.pop_front();
            return payload;
        }
        gone = closed || broken;
        return std::nullopt;
    });
    return 0;
}
