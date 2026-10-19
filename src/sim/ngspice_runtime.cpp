// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "ngspice_runtime.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace simnodus::sim {
namespace {

std::atomic<bool> session_open{false};

#ifdef _WIN32
void* load_library(const std::filesystem::path& file, std::string& error)
{
    // Resolve sndfile/samplerate/OpenMP next to the DLL, never from the
    // current directory or PATH. This search mode needs an absolute path.
    std::error_code ignored;
    const auto absolute = std::filesystem::absolute(file, ignored);
    HMODULE handle = LoadLibraryExW(absolute.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!handle) {
        error = "Cannot load " + file.string() + " (Windows error " + std::to_string(GetLastError()) + ").";
    }
    return reinterpret_cast<void*>(handle);
}

void* find_symbol(void* library, const char* name)
{
    FARPROC address = GetProcAddress(static_cast<HMODULE>(library), name);
    void* result = nullptr;
    static_assert(sizeof(address) == sizeof(result));
    std::memcpy(&result, &address, sizeof(result));
    return result;
}

void free_library(void* library)
{
    FreeLibrary(static_cast<HMODULE>(library));
}

bool set_scripts_variable(const std::filesystem::path& directory)
{
    // Set before loading the DLL so that its C runtime inherits the value.
    return SetEnvironmentVariableW(L"SPICE_SCRIPTS", directory.c_str()) != 0;
}
#else
void* load_library(const std::filesystem::path& file, std::string& error)
{
    void* handle = dlopen(file.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* reason = dlerror();
        error = "Cannot load " + file.string() + ": " + (reason ? reason : "unknown error");
    }
    return handle;
}

void* find_symbol(void* library, const char* name)
{
    return dlsym(library, name);
}

void free_library(void* library)
{
    dlclose(library);
}

bool set_scripts_variable(const std::filesystem::path& directory)
{
    return setenv("SPICE_SCRIPTS", directory.c_str(), 1) == 0;
}
#endif

template <typename Function>
bool bind(void* library, const char* name, Function*& target, std::string& error)
{
    void* address = find_symbol(library, name);
    if (!address) {
        error = std::string("The ngspice library does not export ") + name + ".";
        return false;
    }
    static_assert(sizeof(target) == sizeof(address));
    std::memcpy(&target, &address, sizeof(target));
    return true;
}

} // namespace

std::vector<NgspiceLocation> ngspice_candidates(const std::filesystem::path& application_dir)
{
    std::vector<NgspiceLocation> result;
    const auto bundled = application_dir / "ngspice";
#ifdef _WIN32
    result.push_back({bundled / "ngspice.dll", bundled});
#else
    result.push_back({bundled / "libngspice.so.0", bundled});
    result.push_back({application_dir / ".." / "lib" / "simnodus" / "ngspice" / "libngspice.so.0",
        application_dir / ".." / "share" / "simnodus" / "ngspice"});
    // System library from the distribution, with the owned initialization file.
    result.push_back({"libngspice.so.0", application_dir / ".." / "share" / "simnodus" / "ngspice"});
    result.push_back({"libngspice.so.0", bundled});
#endif
    return result;
}

std::optional<int> parse_ngspice_version(const std::string& text)
{
    static const std::regex pattern(R"(ngspice-(\d+))");
    std::smatch match;
    if (std::regex_search(text, match, pattern)) {
        return std::stoi(match[1].str());
    }
    return std::nullopt;
}

std::unique_ptr<NgspiceSession> NgspiceSession::open(const NgspiceLocation& location, std::string& error)
{
    if (session_open.exchange(true)) {
        error = "An ngspice session is already open in this process.";
        return nullptr;
    }
    std::unique_ptr<NgspiceSession> session(new NgspiceSession());
    auto fail = [&](std::string message) -> std::unique_ptr<NgspiceSession> {
        error = std::move(message);
        session.reset();
        return nullptr;
    };
    // A missing spinit makes ngspice search user locations; the owned file
    // disables that search (tools/backend_probe/initialization/spinit).
    if (!std::filesystem::is_regular_file(location.scripts_dir / "spinit")) {
        return fail("Missing ngspice initialization file: " + (location.scripts_dir / "spinit").string());
    }
    if (!set_scripts_variable(location.scripts_dir)) {
        return fail("Cannot set SPICE_SCRIPTS.");
    }
    void* library = load_library(location.library, error);
    if (!library) {
        return fail(error);
    }
    session->library_ = library;
    auto& api = session->api_;
    if (!bind(library, "ngSpice_Init", api.init, error) || !bind(library, "ngSpice_Init_Sync", api.init_sync, error)
        || !bind(library, "ngSpice_Command", api.command, error)
        || !bind(library, "ngGet_Vec_Info", api.get_vec_info, error)
        || !bind(library, "ngSpice_Circ", api.circ, error) || !bind(library, "ngSpice_CurPlot", api.cur_plot, error)
        || !bind(library, "ngSpice_AllVecs", api.all_vecs, error)
        || !bind(library, "ngSpice_running", api.running, error)) {
        return fail(error);
    }
    if (void* address = find_symbol(library, "ngSpice_SetBkpt")) {
        std::memcpy(&api.set_bkpt, &address, sizeof(api.set_bkpt));
    }
    // SendData callbacks need a non-null SendInitData (E-01 finding 1).
    if (api.init(&on_char, &on_stat, &on_exit, &on_data, &on_init_data, &on_bg_thread, session.get()) != 0) {
        return fail("ngSpice_Init failed.");
    }
    int ident = 0;
    if (api.init_sync(&on_vsrc, nullptr, &on_sync, &ident, session.get()) != 0) {
        return fail("ngSpice_Init_Sync failed.");
    }
    session->command("version -s");
    std::string all;
    for (const auto& line : session->take_output()) {
        all += line + '\n';
    }
    session->version_ = parse_ngspice_version(all);
    return session;
}

NgspiceSession::~NgspiceSession()
{
    if (api_.command && !needs_reload_) {
        halt_background(std::chrono::seconds(5));
    }
    if (library_) {
        free_library(library_); // the next session loads a fresh copy
    }
    session_open = false;
}

void NgspiceSession::set_data_handler(DataHandler handler)
{
    data_handler_ = std::move(handler);
}

void NgspiceSession::set_source_handler(SourceHandler handler)
{
    source_handler_ = std::move(handler);
}

void NgspiceSession::set_step_handler(StepHandler handler)
{
    step_handler_ = std::move(handler);
}

bool NgspiceSession::command(const std::string& text)
{
    std::vector<char> buffer(text.begin(), text.end());
    buffer.push_back('\0');
    return api_.command(buffer.data()) == 0;
}

bool NgspiceSession::set_breakpoint(double time)
{
    return api_.set_bkpt && api_.set_bkpt(time);
}

bool NgspiceSession::load_circuit(const std::vector<std::string>& lines)
{
    std::vector<std::vector<char>> storage;
    std::vector<char*> pointers;
    storage.reserve(lines.size());
    for (const auto& line : lines) {
        storage.emplace_back(line.begin(), line.end());
        storage.back().push_back('\0');
        pointers.push_back(storage.back().data());
    }
    pointers.push_back(nullptr);
    const std::size_t errors_before = error_line_count();
    {
        std::lock_guard lock(mutex_);
        names_valid_ = false;
        names_.clear();
    }
    // A zero return is not proof of success (E-01 finding 6).
    return api_.circ(pointers.data()) == 0 && error_line_count() == errors_before;
}

std::vector<std::string> NgspiceSession::vector_names() const
{
    std::lock_guard lock(mutex_);
    return names_;
}

std::vector<double> NgspiceSession::vector(const std::string& name) const
{
    std::vector<char> buffer(name.begin(), name.end());
    buffer.push_back('\0');
    const ngspice_abi::VectorInfo* info = api_.get_vec_info(buffer.data());
    if (!info || !info->v_realdata || info->v_length <= 0) {
        return {};
    }
    return {info->v_realdata, info->v_realdata + info->v_length};
}

bool NgspiceSession::start_background(const std::string& command_text)
{
    {
        std::lock_guard lock(mutex_);
        bg_running_ = true;
    }
    if (!command(command_text)) {
        std::lock_guard lock(mutex_);
        bg_running_ = false;
        return false;
    }
    return true;
}

bool NgspiceSession::background_running() const
{
    std::lock_guard lock(mutex_);
    return bg_running_;
}

bool NgspiceSession::wait_background(std::chrono::milliseconds timeout)
{
    std::unique_lock lock(mutex_);
    return bg_changed_.wait_for(lock, timeout, [this] { return !bg_running_; });
}

void NgspiceSession::halt_background(std::chrono::milliseconds timeout)
{
    if (!background_running()) {
        return;
    }
    command("bg_halt");
    wait_background(timeout);
}

std::vector<std::string> NgspiceSession::take_output()
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> result;
    result.swap(output_);
    return result;
}

std::size_t NgspiceSession::error_line_count() const
{
    std::lock_guard lock(mutex_);
    return error_lines_;
}

int NgspiceSession::on_char(char* text, int, void* user)
{
    auto* self = static_cast<NgspiceSession*>(user);
    std::string line = text ? text : "";
    std::lock_guard lock(self->mutex_);
    if (line.rfind("stderr", 0) == 0 && line.find("rror") != std::string::npos) {
        ++self->error_lines_;
    }
    if (self->output_.size() < 10000) {
        self->output_.push_back(std::move(line));
    }
    return 0;
}

int NgspiceSession::on_stat(char*, int, void*)
{
    return 0;
}

int NgspiceSession::on_exit(int, ngspice_abi::NgBool, ngspice_abi::NgBool, int, void* user)
{
    // "cannot recover and awaits to be reset or detached": reload before the next use.
    static_cast<NgspiceSession*>(user)->needs_reload_ = true;
    return 0;
}

int NgspiceSession::on_data(ngspice_abi::VecValuesAll* values, int, int, void* user)
{
    auto* self = static_cast<NgspiceSession*>(user);
    if (!values || values->veccount <= 0) {
        return 0;
    }
    {
        std::lock_guard lock(self->mutex_);
        if (!self->names_valid_) {
            self->names_.clear();
            for (int i = 0; i < values->veccount; ++i) {
                self->names_.emplace_back(values->vecsa[i]->name ? values->vecsa[i]->name : "");
            }
            self->names_valid_ = true;
        }
    }
    auto& point = self->scratch_;
    point.index = values->vecindex;
    point.values.resize(static_cast<std::size_t>(values->veccount));
    for (int i = 0; i < values->veccount; ++i) {
        const auto* vec = values->vecsa[i];
        point.values[static_cast<std::size_t>(i)] = vec->creal;
        if (vec->is_scale) {
            point.time = vec->creal;
        }
    }
    if (self->data_handler_) {
        self->data_handler_(point);
    }
    return 0;
}

int NgspiceSession::on_init_data(ngspice_abi::VecInfoAll*, int, void* user)
{
    auto* self = static_cast<NgspiceSession*>(user);
    std::lock_guard lock(self->mutex_);
    self->names_valid_ = false;
    return 0;
}

int NgspiceSession::on_bg_thread(ngspice_abi::NgBool not_running, int, void* user)
{
    // ngspice-47 reports false when the worker starts and true when it has
    // exited (E-01 finding 5).
    auto* self = static_cast<NgspiceSession*>(user);
    {
        std::lock_guard lock(self->mutex_);
        self->bg_running_ = !not_running;
    }
    self->bg_changed_.notify_all();
    return 0;
}

int NgspiceSession::on_vsrc(double* value, double time, char* name, int, void* user)
{
    auto* self = static_cast<NgspiceSession*>(user);
    *value = self->source_handler_ ? self->source_handler_(name ? name : "", time) : 0.0;
    return 0;
}

// sharedsync() in ngspice's sharedspice.c: location 0 after the breakpoint
// handling of a new step, location 1 after it converged (redo: it is retried
// from `time` with `delta`). Returning 0 never asks for an extra redo.
int NgspiceSession::on_sync(double time, double* delta, double, int redo, int, int location, void* user)
{
    auto* self = static_cast<NgspiceSession*>(user);
    if (self->step_handler_ && delta && (location == 0 || redo)) {
        self->step_handler_(time, *delta);
    }
    return 0;
}

} // namespace simnodus::sim
