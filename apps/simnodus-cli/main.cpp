// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// simnodus-cli (SN-215, ADR 0104): SimNodus without a window, for AI agents,
// scripts and CI. Every result is JSON on standard output; whatever the
// engines print goes to standard error. Exit codes: 0 done, 1 a command
// failed (or the circuit check found errors), 2 wrong usage, 3 no component
// library, 4 no window to connect to. Commands: docs/user/AUTOMATION.md.
// "connect" passes the requests to the desktop window instead (SN-216).
#include "automation/automation.hpp"
#include "automation/mcp.hpp"
#include "sim/engine_host.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <memory>
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
#include <sys/socket.h>
#include <sys/un.h>
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
  connect [SCRIPT]         send the requests (standard input, or a file) to the
                           SimNodus window, whose external control must be on
                           (Tools > Allow External Control, or simnodus --control)
  mcp [--window]           MCP server on standard input and output, for AI agents;
                           with --window the commands act on the SimNodus window

options:
  --pretty                 indent the JSON output
  --library DIR            component library (default: next to the program)
  --ngspice LIB            ngspice shared library (default: the bundled one)
  --ngspice-scripts DIR    folder with ngspice's spinit
  --keep-going             exec, connect: go on after a failed request
  --window                 mcp: control the SimNodus window
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

// An environment variable as UTF-8; empty if not set.
std::string environment(const char* name)
{
#ifdef _WIN32
    const std::wstring wide_name(name, name + std::strlen(name));
    const DWORD size = GetEnvironmentVariableW(wide_name.c_str(), nullptr, 0);
    if (size == 0) {
        return {};
    }
    std::wstring value(size, L'\0');
    value.resize(GetEnvironmentVariableW(wide_name.c_str(), value.data(), size));
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string text(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), text.data(), bytes, nullptr, nullptr);
    return text;
#else
    const char* value = std::getenv(name);
    return value ? value : "";
#endif
}

// The window's control endpoint (SN-216): a named pipe on Windows, a local
// socket elsewhere, named after the user like the window's QLocalServer.
// SIMNODUS_CONTROL_NAME overrides the name (tests, several windows).
std::string control_endpoint()
{
    std::string name = environment("SIMNODUS_CONTROL_NAME");
    if (name.empty()) {
#ifdef _WIN32
        const std::string user = environment("USERNAME");
#else
        const std::string user = environment("USER");
#endif
        // As the window builds it: letters, digits, '-' and '_' kept, and one
        // '_' for every other UTF-16 unit.
        name = "simnodus-control-";
        for (std::size_t i = 0; i < user.size(); ++i) {
            const auto u = static_cast<unsigned char>(user[i]);
            if (u < 0x80) {
                name += std::isalnum(u) || u == '-' || u == '_' ? static_cast<char>(u) : '_';
            } else if (u >= 0xC0) {
                name += u >= 0xF0 ? "__" : "_"; // a lead byte; four-byte sequences are surrogate pairs
            }
        }
    }
#ifdef _WIN32
    return R"(\\.\pipe\)" + name;
#else
    if (name.find('/') != std::string::npos) {
        return name;
    }
    std::string dir = environment("TMPDIR");
    if (dir.empty()) {
        dir = "/tmp";
    }
    while (dir.size() > 1 && dir.back() == '/') {
        dir.pop_back();
    }
    return dir + "/" + name;
#endif
}

// A connection to the window: one line of JSON out, one line back.
class WindowLink {
public:
    ~WindowLink()
    {
#ifdef _WIN32
        if (pipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_);
        }
#else
        if (socket_ >= 0) {
            close(socket_);
        }
#endif
    }

    bool open(const std::string& endpoint)
    {
#ifdef _WIN32
        const int size = MultiByteToWideChar(CP_UTF8, 0, endpoint.c_str(), -1, nullptr, 0);
        std::wstring wide(static_cast<std::size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, endpoint.c_str(), -1, wide.data(), size);
        for (int attempt = 0; attempt < 2; ++attempt) {
            pipe_ = CreateFileW(wide.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (pipe_ != INVALID_HANDLE_VALUE || GetLastError() != ERROR_PIPE_BUSY || !WaitNamedPipeW(wide.c_str(), 5000)) {
                break;
            }
        }
        return pipe_ != INVALID_HANDLE_VALUE;
#else
        sockaddr_un address{};
        if (endpoint.size() >= sizeof(address.sun_path)) {
            return false;
        }
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, endpoint.c_str(), endpoint.size() + 1);
        socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
        return socket_ >= 0 && connect(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
#endif
    }

    // Sends a request and returns the reply's line; nothing if the window went away.
    std::optional<std::string> ask(const std::string& line)
    {
        const std::string text = line + "\n";
        std::size_t done = 0;
        while (done < text.size()) {
            const auto n = put(text.data() + done, text.size() - done);
            if (n <= 0) {
                return std::nullopt;
            }
            done += static_cast<std::size_t>(n);
        }
        for (;;) {
            if (const auto end = buffer_.find('\n'); end != std::string::npos) {
                std::string reply = buffer_.substr(0, end);
                buffer_.erase(0, end + 1);
                return reply;
            }
            char chunk[4096];
            const auto n = get(chunk, sizeof(chunk));
            if (n <= 0) {
                return std::nullopt;
            }
            buffer_.append(chunk, static_cast<std::size_t>(n));
        }
    }

private:
#ifdef _WIN32
    long long put(const char* data, std::size_t size)
    {
        DWORD n = 0;
        return WriteFile(pipe_, data, static_cast<DWORD>(size), &n, nullptr) ? static_cast<long long>(n) : -1;
    }
    long long get(char* data, std::size_t size)
    {
        DWORD n = 0;
        return ReadFile(pipe_, data, static_cast<DWORD>(size), &n, nullptr) ? static_cast<long long>(n) : -1;
    }
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
#else
    long long put(const char* data, std::size_t size) { return ::write(socket_, data, size); }
    long long get(char* data, std::size_t size) { return ::read(socket_, data, size); }
    int socket_ = -1;
#endif
    std::string buffer_;
};

json no_window(const std::string& endpoint)
{
    return {{"ok", false}, {"error", {{"code", "no-window"},
        {"message", "No SimNodus window accepts external control at " + endpoint +
                ". Start SimNodus with --control, or turn on Tools > Allow External Control."}}}};
}

// Requests for the window over a connection opened when first needed and
// opened again if the window was closed and started anew.
class Window {
public:
    json execute(const json& request)
    {
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (!link_) {
                auto link = std::make_unique<WindowLink>();
                if (!link->open(endpoint_)) {
                    break;
                }
                // Relative paths in the requests start from this folder, as they would here.
                const auto here = std::filesystem::current_path().u8string();
                link->ask(json{{"command", "hello"}, {"args", {{"directory", std::string(here.begin(), here.end())}}}}.dump());
                link_ = std::move(link);
            }
            if (const auto reply = link_->ask(request.dump(-1, ' ', false, json::error_handler_t::replace))) {
                try {
                    return json::parse(*reply);
                } catch (const json::exception&) {
                    return {{"ok", false}, {"error", {{"code", "internal"}, {"message", "The window sent a reply that is not JSON."}}}};
                }
            }
            link_.reset();
        }
        if (request.value("command", "") == "help") {
            return {{"ok", true}, {"result", simnodus::automation::Automation::help()}}; // tools to list meanwhile
        }
        return no_window(endpoint_);
    }

private:
    std::string endpoint_ = control_endpoint();
    std::unique_ptr<WindowLink> link_;
};

// MCP over standard input and output: one JSON-RPC message per line.
int mcp(const simnodus::automation::McpServer::Execute& execute)
{
    simnodus::automation::McpServer server(execute, SIMNODUS_CLI_VERSION);
    pretty = false; // one message per line
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }
        if (const auto response = server.handle_line(line)) {
            emit(*response);
        }
    }
    return 0;
}

int lines(const std::function<json(const std::string&)>& execute, std::istream& in, bool stop_on_failure)
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
        const auto reply = execute(line);
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
    bool window = false;
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
        } else if (a == "--window") {
            window = true;
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

    if (command == "connect") {
        // Nothing runs here: the window does the work, so no library or engine is needed.
        std::fflush(stdout);
        out_fd = dup(fileno(stdout));
        std::ifstream script;
        if (positional.size() > 1) {
            script.open(path_from(positional[1]));
            if (!script) {
                std::fprintf(stderr, "cannot read %s\n", positional[1].c_str());
                return 2;
            }
        }
        const auto endpoint = control_endpoint();
        WindowLink link;
        if (!link.open(endpoint)) {
            emit(no_window(endpoint));
            return 4;
        }
        // Relative paths in the requests start from this folder, as they would here.
        const auto here = std::filesystem::current_path().u8string();
        link.ask(json{{"command", "hello"}, {"args", {{"directory", std::string(here.begin(), here.end())}}}}.dump());
        bool lost = false;
        const auto send = [&](const std::string& line) -> json {
            const auto reply = lost ? std::nullopt : link.ask(line);
            if (!reply) {
                lost = true;
                return {{"ok", false}, {"error", {{"code", "no-window"}, {"message", "The SimNodus window closed the connection."}}}};
            }
            try {
                return json::parse(*reply);
            } catch (const json::exception&) {
                return {{"ok", false}, {"error", {{"code", "internal"}, {"message", "The window sent a reply that is not JSON."}}}};
            }
        };
        const int result = positional.size() > 1 ? lines(send, script, !keep_going) : (lines(send, std::cin, false), 0);
        return lost ? 4 : result;
    }

#ifdef _WIN32
    _setmode(fileno(stdout), _O_BINARY);
    _setmode(fileno(stdin), _O_BINARY);
#endif
    if (command == "mcp" && window) {
        std::fflush(stdout);
        out_fd = dup(fileno(stdout));
        dup2(fileno(stderr), fileno(stdout));
        Window target;
        return mcp([&](const json& request) { return target.execute(request); });
    }
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

    if (command == "mcp") {
        return mcp([&](const json& request) { return automation.execute(request); });
    }
    if (command == "serve") {
        if (positional.size() > 1 && !open_project(automation, positional[1])) {
            return 1;
        }
        lines([&](const std::string& line) { return automation.execute_line(line); }, std::cin, false);
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
        return lines([&](const std::string& line) { return automation.execute_line(line); }, script, !keep_going);
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
