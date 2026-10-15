// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "renode_engine.hpp"

#include "child_process.hpp"

#include "renode_api_simnodus.h"
#include "transport.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <list>
#include <map>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>

namespace simnodus::mcu {

namespace fs = std::filesystem;

namespace {

std::string environment(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    std::size_t size = 0;
    std::string result;
    if (_dupenv_s(&value, &size, name) == 0 && value) {
        result = value;
    }
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? value : "";
#endif
}

#ifdef _WIN32
constexpr const char* renode_name = "renode.exe";
#else
constexpr const char* renode_name = "renode";
#endif

constexpr const char* machine_name = "mcu";
constexpr const char* server_file = "LoopbackControl.cs";

std::optional<fs::path> executable_in(const fs::path& folder)
{
    std::error_code ec;
    if (fs::is_regular_file(folder / renode_name, ec)) {
        return folder / renode_name;
    }
    for (const auto& entry : fs::directory_iterator(folder, ec)) {
        if (entry.is_directory(ec) && fs::is_regular_file(entry.path() / renode_name, ec)) {
            return entry.path() / renode_name;
        }
    }
    return std::nullopt;
}

// Lines of the log that mean the monitor script failed.
bool startup_failed(const std::string& log)
{
    for (const char* marker : {"Errors during", "Fatal error", "error executing command", "Unhandled exception"}) {
        if (log.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string read_file(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

std::string unique_suffix()
{
    static std::atomic<unsigned> counter{0};
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    std::ostringstream text;
    text << std::chrono::duration_cast<std::chrono::milliseconds>(now).count() << '-' << ++counter;
    return text.str();
}

// Run folders of failed runs are kept for their logs; remove those older than a day.
void prune_old_runs(const fs::path& base)
{
    std::error_code ec;
    const auto limit = fs::file_time_type::clock::now() - std::chrono::hours(24);
    for (const auto& entry : fs::directory_iterator(base, ec)) {
        if (entry.path().filename().string().rfind("renode-", 0) == 0 && entry.last_write_time(ec) < limit) {
            fs::remove_all(entry.path(), ec);
        }
    }
}

} // namespace

std::optional<fs::path> find_renode(const fs::path& application_dir)
{
    if (const std::string explicit_path = environment("SIMNODUS_RENODE"); !explicit_path.empty()) {
        std::error_code ec;
        if (fs::is_regular_file(explicit_path, ec)) {
            return fs::path(explicit_path);
        }
        return executable_in(explicit_path);
    }
    if (auto found = executable_in(application_dir / "renode")) {
        return found;
    }
#ifdef SIMNODUS_DEV_RENODE_DIR
    return executable_in(SIMNODUS_DEV_RENODE_DIR);
#else
    return std::nullopt;
#endif
}

std::optional<fs::path> find_mcu_resources(const fs::path& application_dir)
{
    std::error_code ec;
    for (const fs::path& folder : {application_dir / "mcu", application_dir.parent_path() / "share/simnodus/mcu",
             fs::path(SIMNODUS_SOURCE_MCU_DIR)}) {
        if (fs::is_regular_file(folder / server_file, ec)) {
            return folder;
        }
    }
    return std::nullopt;
}

struct RenodeEngine::Impl {
    struct Watch {
        Impl* owner;
        std::string port;
        int pin;
    };

    std::unique_ptr<ChildProcess> process;
    renode_t* renode = nullptr;
    renode_machine_t* machine = nullptr;
    renode_bus_context_t* sysbus = nullptr;
    std::map<std::string, renode_gpio_t*> ports;
    std::list<Watch> watches; // stable addresses for the callbacks
    std::vector<GpioEvent> events;
    struct UartWatch {
        Impl* owner;
        std::string uart;
    };
    std::map<std::string, renode_uart_t*> uarts;
    std::list<UartWatch> uart_watches;
    std::vector<UartByte> uart_bytes;

    static void on_uart(void* user, renode_uart_event_data_t* data)
    {
        auto* watch = static_cast<UartWatch*>(user);
        try {
            watch->owner->uart_bytes.push_back({data->timestamp_us, watch->uart, data->byte});
        } catch (...) {
            watch->owner->broken = true;
        }
    }
    bool broken = false;

    static void on_gpio(void* user, renode_gpio_event_data_t* data)
    {
        auto* watch = static_cast<Watch*>(user);
        try {
            watch->owner->events.push_back({data->timestamp_us, watch->port, watch->pin, data->state});
        } catch (...) {
            watch->owner->broken = true; // out of memory: the run cannot be trusted any more
        }
    }

    void disconnect()
    {
        for (auto& [name, gpio] : ports) {
            std::free(gpio);
        }
        ports.clear();
        for (auto& [name, uart] : uarts) {
            std::free(uart);
        }
        uarts.clear();
        std::free(sysbus);
        sysbus = nullptr;
        std::free(machine);
        machine = nullptr;
        if (renode) {
            if (renode_error_t* failure = renode_disconnect(&renode)) {
                renode_free_error(failure);
            }
        }
    }
};

RenodeEngine::RenodeEngine() : impl_(std::make_unique<Impl>()) {}

RenodeEngine::~RenodeEngine()
{
    stop(std::chrono::milliseconds(5000));
}

std::unique_ptr<RenodeEngine> RenodeEngine::start(const MachineOptions& options, std::string& error)
{
    std::error_code ec;
    if (!fs::is_regular_file(options.renode, ec)) {
        error = "Renode was not found at " + options.renode.string() + ".";
        return nullptr;
    }
    std::vector<std::string> needed{server_file, options.platform};
    needed.insert(needed.end(), options.extensions.begin(), options.extensions.end());
    for (const auto& name : needed) {
        // Plain file names only: they are copied into the run folder and named in monitor commands.
        if (name.find_first_of("/\\ ;\"") != std::string::npos || !fs::is_regular_file(options.resources / name, ec)) {
            error = "The microcontroller support file " + name + " is missing in " + options.resources.string() + ".";
            return nullptr;
        }
    }
    if (!options.firmware.empty() && !fs::is_regular_file(options.firmware, ec)) {
        error = "The firmware file " + options.firmware.string() + " does not exist.";
        return nullptr;
    }

    std::unique_ptr<RenodeEngine> engine(new RenodeEngine);
    // Monitor `include @file` cannot quote paths, so every file Renode reads is
    // copied into the run folder and named relative to it.
    const fs::path base = fs::temp_directory_path(ec) / "SimNodus";
    prune_old_runs(base);
    engine->run_dir_ = base / ("renode-" + unique_suffix());
    fs::create_directories(engine->run_dir_ / "tmp", ec);
    if (ec) {
        error = "Cannot create the folder " + engine->run_dir_.string() + ": " + ec.message();
        return nullptr;
    }
    fs::copy_file(options.resources / server_file, engine->run_dir_ / server_file, ec);
    if (!ec) {
        fs::copy_file(options.resources / options.platform, engine->run_dir_ / "platform.repl", ec);
    }
    for (const auto& name : options.extensions) {
        if (!ec) {
            fs::copy_file(options.resources / name, engine->run_dir_ / name, ec);
        }
    }
    if (!ec && !options.firmware.empty()) {
        fs::copy_file(options.firmware, engine->run_dir_ / "firmware.elf", ec);
    }
    if (ec) {
        error = "Cannot prepare the microcontroller run: " + ec.message();
        return nullptr;
    }

    const std::uint16_t port = sn_free_loopback_port();
    if (port == 0) {
        error = "No free loopback port for the microcontroller engine.";
        return nullptr;
    }
    std::vector<std::string> script{std::string("include @") + server_file};
    for (const auto& name : options.extensions) {
        script.push_back("include @" + name);
    }
    script.push_back(std::string("mach create \"") + machine_name + '"');
    script.emplace_back("machine LoadPlatformDescription @platform.repl");
    script.insert(script.end(), options.before_firmware.begin(), options.before_firmware.end());
    if (!options.firmware.empty()) {
        script.emplace_back("sysbus LoadELF @firmware.elf");
    }
    script.insert(script.end(), options.after_firmware.begin(), options.after_firmware.end());
    {
        std::ostringstream quantum;
        quantum.setf(std::ios::fixed);
        quantum.precision(6);
        quantum << "emulation SetGlobalQuantum \"" << options.quantum_us / 1e6 << '"';
        script.push_back(quantum.str());
    }
    script.push_back("emulation CreateLoopbackControlServer \"control\" " + std::to_string(port));
    std::string joined;
    for (const auto& command : script) {
        joined += joined.empty() ? command : "; " + command;
    }

    ProcessOptions process;
    process.executable = options.renode;
    process.arguments = {"--disable-xwt", "--console", "--plain", "--config",
        (engine->run_dir_ / "renode.config").string(), "--execute", joined};
    process.working_directory = engine->run_dir_;
    const std::string temp = (engine->run_dir_ / "tmp").string();
    // A fresh temp folder: Renode's temp cleanup otherwise inspects stale processes (SN-019).
    process.environment = {{"TEMP", temp}, {"TMP", temp}, {"TMPDIR", temp}};
    process.output_file = engine->log_file();
    engine->impl_->process = ChildProcess::start(process, error);
    if (!engine->impl_->process) {
        return nullptr;
    }

    const auto deadline = std::chrono::steady_clock::now() + options.start_timeout;
    const std::string port_text = std::to_string(port);
    for (;;) {
        if (!engine->impl_->process->running()) {
            error = "Renode exited while starting.\n" + engine->log_tail();
            return nullptr;
        }
        if (options.cancel && *options.cancel) {
            error = "The start was cancelled.";
            return nullptr;
        }
        if (startup_failed(read_file(engine->log_file()))) {
            error = "Renode could not set up the microcontroller.\n" + engine->log_tail();
            return nullptr;
        }
        if (read_file(engine->log_file()).find("Listening on port " + port_text) != std::string::npos) {
            renode_error_t* failure = renode_connect(port_text.c_str(), &engine->impl_->renode);
            if (!failure) {
                break;
            }
            renode_free_error(failure);
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            error = "Renode did not become ready within "
                + std::to_string(options.start_timeout.count() / 1000) + " s.\n" + engine->log_tail();
            return nullptr;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!engine->check(renode_get_machine(engine->impl_->renode, machine_name, &engine->impl_->machine),
            "Cannot find the microcontroller machine")
        || !engine->check(renode_get_sysbus(engine->impl_->machine, &engine->impl_->sysbus),
            "Cannot reach the system bus")) {
        error = engine->error_;
        return nullptr;
    }
    return engine;
}

bool RenodeEngine::check(void* renode_error, const char* what)
{
    auto* failure = static_cast<renode_error_t*>(renode_error);
    if (!failure) {
        return true;
    }
    error_ = std::string(what) + ": " + (failure->message ? failure->message : "no details");
    // A failed command keeps the connection; everything else retires it.
    if (failure->code != ERR_COMMAND_FAILED) {
        impl_->broken = true;
    }
    renode_free_error(failure);
    return false;
}

bool RenodeEngine::usable() const
{
    return impl_->renode && !impl_->broken && impl_->process && impl_->process->running();
}

std::optional<std::uint64_t> RenodeEngine::time_us()
{
    if (!usable()) {
        return std::nullopt;
    }
    std::uint64_t now = 0;
    if (!check(renode_get_current_time(impl_->renode, TU_MICROSECONDS, &now), "Cannot read the virtual time")) {
        return std::nullopt;
    }
    return now;
}

bool RenodeEngine::run_for(std::uint64_t microseconds)
{
    if (!usable()) {
        return false;
    }
    // At least 10 s, plus 100 ms of wall time per virtual millisecond.
    const std::uint64_t limit = std::min<std::uint64_t>(10000 + microseconds / 10, 3600000);
    renode_set_operation_timeout(impl_->renode, static_cast<std::uint32_t>(limit));
    const bool ok = check(renode_run_for(impl_->renode, TU_MICROSECONDS, microseconds), "Renode failed to run");
    if (impl_->renode) {
        renode_set_operation_timeout(impl_->renode, 0);
    }
    return ok && !impl_->broken;
}

void* RenodeEngine::gpio_handle(const std::string& port)
{
    if (!usable()) {
        return nullptr;
    }
    if (const auto it = impl_->ports.find(port); it != impl_->ports.end()) {
        return it->second;
    }
    renode_gpio_t* gpio = nullptr;
    if (!check(renode_get_gpio(impl_->machine, port.c_str(), &gpio), ("Cannot find " + port).c_str())) {
        return nullptr;
    }
    impl_->ports[port] = gpio;
    return gpio;
}

bool RenodeEngine::watch_gpio(const std::string& port, int pin)
{
    auto* gpio = static_cast<renode_gpio_t*>(gpio_handle(port));
    if (!gpio) {
        return false;
    }
    auto& watch = impl_->watches.emplace_back(Impl::Watch{impl_.get(), port, pin});
    return check(renode_register_gpio_state_change_callback(gpio, pin, &watch, &Impl::on_gpio),
        ("Cannot watch " + port + " pin " + std::to_string(pin)).c_str());
}

std::vector<GpioEvent> RenodeEngine::take_gpio_events()
{
    return std::exchange(impl_->events, {});
}

bool RenodeEngine::set_gpio(const std::string& port, int pin, bool state)
{
    auto* gpio = static_cast<renode_gpio_t*>(gpio_handle(port));
    return gpio
        && check(renode_set_gpio_state(gpio, pin, state), ("Cannot set " + port + " pin " + std::to_string(pin)).c_str());
}

std::optional<bool> RenodeEngine::gpio(const std::string& port, int pin)
{
    auto* gpio = static_cast<renode_gpio_t*>(gpio_handle(port));
    bool state = false;
    if (!gpio
        || !check(renode_get_gpio_state(gpio, pin, &state), ("Cannot read " + port + " pin " + std::to_string(pin)).c_str())) {
        return std::nullopt;
    }
    return state;
}

void* RenodeEngine::uart_handle(const std::string& uart)
{
    if (!usable()) {
        return nullptr;
    }
    if (const auto it = impl_->uarts.find(uart); it != impl_->uarts.end()) {
        return it->second;
    }
    renode_uart_t* handle = nullptr;
    if (!check(renode_get_uart(impl_->machine, uart.c_str(), &handle), ("Cannot find " + uart).c_str())) {
        return nullptr;
    }
    impl_->uarts[uart] = handle;
    return handle;
}

bool RenodeEngine::watch_uart(const std::string& uart)
{
    auto* handle = static_cast<renode_uart_t*>(uart_handle(uart));
    if (!handle) {
        return false;
    }
    auto& watch = impl_->uart_watches.emplace_back(Impl::UartWatch{impl_.get(), uart});
    return check(renode_register_uart_callback(handle, &watch, &Impl::on_uart), ("Cannot watch " + uart).c_str());
}

std::vector<UartByte> RenodeEngine::take_uart_bytes()
{
    return std::exchange(impl_->uart_bytes, {});
}

bool RenodeEngine::write_uart(const std::string& uart, const std::vector<std::uint8_t>& bytes)
{
    auto* handle = static_cast<renode_uart_t*>(uart_handle(uart));
    return handle
        && check(renode_uart_write(handle, bytes.data(), static_cast<std::uint32_t>(bytes.size())),
            ("Cannot write to " + uart).c_str());
}

std::optional<std::uint32_t> RenodeEngine::read32(std::uint64_t address)
{
    std::uint32_t value = 0;
    if (!usable() || !check(renode_sysbus_read(impl_->sysbus, address, AW_DOUBLE_WORD, &value, 1), "Bus read failed")) {
        return std::nullopt;
    }
    return value;
}

bool RenodeEngine::write32(std::uint64_t address, std::uint32_t value)
{
    return usable() && check(renode_sysbus_write(impl_->sysbus, address, AW_DOUBLE_WORD, &value, 1), "Bus write failed");
}

std::optional<int> RenodeEngine::stop(std::chrono::milliseconds timeout)
{
    impl_->disconnect();
    if (!impl_->process) {
        return std::nullopt;
    }
    impl_->process->write_input("quit\n");
    impl_->process->close_input();
    std::optional<int> code = impl_->process->wait(timeout);
    if (!code) {
        impl_->process->kill();
    }
    impl_->process.reset();
    if (code == 0 && !impl_->broken) {
        std::error_code ec;
        fs::remove_all(run_dir_, ec);
    }
    return code;
}

long long RenodeEngine::process_id() const
{
    return impl_->process ? impl_->process->id() : 0;
}

std::string RenodeEngine::log_tail(std::size_t lines) const
{
    const std::string text = read_file(log_file());
    std::deque<std::string> last;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        last.push_back(line);
        if (last.size() > lines) {
            last.pop_front();
        }
    }
    std::string result;
    for (const auto& line : last) {
        result += line + '\n';
    }
    return result;
}

} // namespace simnodus::mcu
