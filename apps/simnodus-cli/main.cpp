// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// simnodus-cli (SN-215, ADR 0104): SimNodus without a window, for AI agents,
// scripts and CI. Every result is JSON on standard output; whatever the
// engines print goes to standard error. Exit codes: 0 done, 1 a command
// failed (or the circuit check found errors), 2 wrong usage, 3 no component
// library. Commands: docs/user/AUTOMATION.md.
#include "automation/automation.hpp"
#include "sim/engine_host.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define dup _dup
#define dup2 _dup2
#define write _write
#define fileno _fileno
#else
#include <unistd.h>
#endif

#ifndef SIMNODUS_CLI_VERSION
#define SIMNODUS_CLI_VERSION "dev"
#endif

using simnodus::automation::json;

namespace {

const char* usage = R"usage(usage: simnodus-cli [options] <command> [arguments]

commands:
  serve [PROJECT]          read JSON requests from standard input, one per line,
                           and answer each with one line of JSON
  exec SCRIPT [PROJECT]    run the requests of a file (one per line); stops at the
                           first failure unless --keep-going
  parts [SEARCH]           list the parts of the library
  describe TYPE            pins, parameters and size of a part
  inspect PROJECT          components, pins, nets, wires and probes of a project
  check PROJECT            pre-run checks (exit 1 if the circuit has errors)
  run PROJECT [--mode interactive|transient] [--time 20m] [--signal "V(OUT)"]...
              [--event "5m SW1 1"]... [--points N] [--csv FILE]
                           simulate and measure the signals
  commands                 the JSON commands with their arguments

options:
  --pretty                 indent the JSON output
  --library DIR            component library (default: next to the program)
  --ngspice LIB            ngspice shared library (default: the bundled one)
  --ngspice-scripts DIR    folder with ngspice's spinit
  --keep-going             exec: go on after a failed request
  --version                print the version
)usage";

int out_fd = 1;
bool pretty = false;

void emit(const json& value)
{
    const std::string text = (pretty ? value.dump(2) : value.dump()) + "\n";
    std::size_t done = 0;
    while (done < text.size()) {
        const auto n = write(out_fd, text.data() + done, static_cast<unsigned>(text.size() - done));
        if (n <= 0) {
            return;
        }
        done += static_cast<std::size_t>(n);
    }
}

std::filesystem::path executable_dir()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n < buffer.size()) {
            buffer.resize(n);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path();
#else
    std::error_code ec;
    const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
    return ec ? std::filesystem::current_path() : self.parent_path();
#endif
}

std::filesystem::path path_from(const std::string& utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

// Runs one request and prints its reply; true if it succeeded.
bool step(simnodus::automation::Automation& automation, const json& request)
{
    const auto reply = automation.execute(request);
    emit(reply);
    return reply.value("ok", false);
}

bool open_project(simnodus::automation::Automation& automation, const std::string& file)
{
    return step(automation, {{"command", "project.open"}, {"args", {{"path", file}}}});
}

int lines(simnodus::automation::Automation& automation, std::istream& in, bool stop_on_failure)
{
    bool all_ok = true;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.find_first_not_of(" \t") == std::string::npos || line[line.find_first_not_of(" \t")] == '#') {
            continue;
        }
        const auto reply = automation.execute_line(line);
        emit(reply);
        if (!reply.value("ok", false)) {
            all_ok = false;
            if (stop_on_failure) {
                return 1;
            }
        }
    }
    return all_ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    std::optional<std::filesystem::path> library_dir;
    std::optional<std::filesystem::path> ngspice;
    std::optional<std::filesystem::path> ngspice_scripts;
    bool keep_going = false;
    std::vector<std::string> positional;
    // run's options
    std::optional<std::string> mode;
    std::optional<std::string> time;
    std::optional<std::string> csv;
    std::optional<int> points;
    std::vector<std::string> signals;
    std::vector<std::string> events;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& a = args[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= args.size()) {
                std::fprintf(stderr, "%s needs a value\n%s", a.c_str(), usage);
                std::exit(2);
            }
            return args[++i];
        };
        if (a == "--version") {
            std::printf("simnodus-cli %s, protocol %d\n", SIMNODUS_CLI_VERSION, simnodus::automation::protocol_version);
            return 0;
        } else if (a == "--help" || a == "-h") {
            std::printf("%s", usage);
            return 0;
        } else if (a == "--pretty") {
            pretty = true;
        } else if (a == "--keep-going") {
            keep_going = true;
        } else if (a == "--library") {
            library_dir = path_from(value());
        } else if (a == "--ngspice") {
            ngspice = path_from(value());
        } else if (a == "--ngspice-scripts") {
            ngspice_scripts = path_from(value());
        } else if (a == "--mode") {
            mode = value();
        } else if (a == "--time") {
            time = value();
        } else if (a == "--signal") {
            signals.push_back(value());
        } else if (a == "--event") {
            events.push_back(value());
        } else if (a == "--points") {
            points = std::atoi(value().c_str());
        } else if (a == "--csv") {
            csv = value();
        } else if (a.size() > 1 && a[0] == '-' && a[1] == '-') {
            std::fprintf(stderr, "unknown option %s\n%s", a.c_str(), usage);
            return 2;
        } else {
            positional.push_back(a);
        }
    }
    if (positional.empty()) {
        std::fprintf(stderr, "%s", usage);
        return 2;
    }
    const std::string command = positional[0];

#ifdef _WIN32
    _setmode(fileno(stdout), _O_BINARY);
#endif
    // Results get their own handle of standard output; descriptor 1 (printf,
    // ngspice, the emulator) then writes to standard error.
    std::fflush(stdout);
    out_fd = dup(fileno(stdout));
    dup2(fileno(stderr), fileno(stdout));

    const auto app_dir = executable_dir();
    auto& host = simnodus::sim::EngineHost::instance();
    host.set_application_dir(app_dir);
    if (ngspice) {
        host.set_location({*ngspice, ngspice_scripts.value_or(ngspice->parent_path())});
    }

    simnodus::library::ComponentLibrary library;
    std::vector<std::filesystem::path> folders;
    if (library_dir) {
        folders.push_back(*library_dir);
    } else {
        folders = simnodus::library::library_candidates(app_dir);
    }
    for (const auto& folder : folders) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec)) {
            continue;
        }
        for (const auto& m : library.load_directory(folder)) {
            std::fprintf(stderr, "component file %s skipped: %s\n", m.file.string().c_str(), m.text.c_str());
        }
        if (!library.empty()) {
            break;
        }
    }
    if (library.empty()) {
        emit({{"ok", false}, {"error", {{"code", "no-library"}, {"message", "The component library was not found."}}}});
        return 3;
    }

    simnodus::automation::HeadlessWorkspace workspace(library);
    simnodus::automation::Automation automation(workspace);
    auto need = [&](std::size_t count) {
        if (positional.size() < count + 1) {
            std::fprintf(stderr, "%s needs %zu argument(s)\n%s", command.c_str(), count, usage);
            std::exit(2);
        }
    };

    if (command == "serve") {
        if (positional.size() > 1 && !open_project(automation, positional[1])) {
            return 1;
        }
        lines(automation, std::cin, false);
        return 0;
    }
    if (command == "exec") {
        need(1);
        std::ifstream script(path_from(positional[1]));
        if (!script) {
            std::fprintf(stderr, "cannot read %s\n", positional[1].c_str());
            return 2;
        }
        if (positional.size() > 2 && !open_project(automation, positional[2])) {
            return 1;
        }
        return lines(automation, script, !keep_going);
    }
    if (command == "commands") {
        return step(automation, {{"command", "help"}}) ? 0 : 1;
    }
    if (command == "parts") {
        json a = json::object();
        if (positional.size() > 1) {
            a["search"] = positional[1];
        }
        return step(automation, {{"command", "parts.list"}, {"args", a}}) ? 0 : 1;
    }
    if (command == "describe") {
        need(1);
        return step(automation, {{"command", "parts.describe"}, {"args", {{"type", positional[1]}}}}) ? 0 : 1;
    }
    if (command == "inspect" || command == "check" || command == "run") {
        need(1);
        // The project is opened quietly; its reply is printed only on failure.
        const auto opened = automation.execute({{"command", "project.open"}, {"args", {{"path", positional[1]}}}});
        if (!opened.value("ok", false)) {
            emit(opened);
            return 1;
        }
        if (command == "inspect") {
            return step(automation, {{"command", "circuit.inspect"}}) ? 0 : 1;
        }
        if (command == "check") {
            const auto reply = automation.execute({{"command", "circuit.check"}});
            emit(reply);
            return reply.value("ok", false) && reply["result"].value("ok", false) ? 0 : 1;
        }
        json a = json::object();
        if (mode) {
            a["mode"] = *mode;
        }
        if (time) {
            a["duration"] = *time;
        }
        if (!signals.empty()) {
            a["signals"] = signals;
        }
        if (points) {
            a["points"] = *points;
        }
        if (csv) {
            a["csv"] = *csv;
        }
        if (!events.empty()) {
            json list = json::array();
            for (const auto& e : events) {
                // "5m SW1 1": time, component, value
                const auto first = e.find(' ');
                const auto second = first == std::string::npos ? std::string::npos : e.find(' ', first + 1);
                if (second == std::string::npos) {
                    std::fprintf(stderr, "an event is \"TIME COMPONENT VALUE\", like \"5m SW1 1\"\n");
                    return 2;
                }
                list.push_back({{"at", e.substr(0, first)}, {"component", e.substr(first + 1, second - first - 1)},
                    {"value", e.substr(second + 1)}});
            }
            a["events"] = list;
        }
        if (!time && !mode) {
            // Without a duration, a project in interactive mode runs 10 ms.
            const auto settings = automation.execute({{"command", "settings.get"}});
            if (settings["result"].value("mode", "") == "interactive") {
                a["duration"] = 0.01;
            }
        } else if (!time && mode && *mode == "interactive") {
            a["duration"] = 0.01;
        }
        return step(automation, {{"command", "sim.run"}, {"args", a}}) ? 0 : 1;
    }
    std::fprintf(stderr, "unknown command %s\n%s", command.c_str(), usage);
    return 2;
}
