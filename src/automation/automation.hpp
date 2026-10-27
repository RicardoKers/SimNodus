// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Automation commands (SN-215, ADR 0104): one vocabulary of JSON commands for
// AI agents, scripts and tests, used by simnodus-cli today and later by the
// desktop application's control channel and an MCP server. A request is
// {"id": any, "command": "component.add", "args": {...}}; the reply is
// {"id": same, "ok": true, "result": {...}} or
// {"id": same, "ok": false, "error": {"code": "not-found", "message": "..."}}.
// Error codes are stable; messages are for people. Commands and their
// arguments: docs/user/AUTOMATION.md, or the "help" command.
//
// Commands act on a Workspace: the open project, the library its parts come
// from and the simulation session. Edits, files and runs go through it, so the
// desktop window (SN-216) puts them on its undo stack and shows them.
#pragma once

#include "library/component_library.hpp"
#include "model/edit.hpp"
#include "model/schematic.hpp"
#include "sim/simulation_session.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace simnodus::automation {

using nlohmann::json;

// Version of the command vocabulary; grows when a command or a result changes.
inline constexpr int protocol_version = 1;

// A failed command: a stable code and a message for people. Commands, the
// workspace and extensions throw it; execute() turns it into the reply.
struct Error {
    std::string code;
    std::string message;
};

class Workspace {
public:
    virtual ~Workspace() = default;

    virtual const model::Schematic& schematic() const = 0;
    // Only for reserving IDs in editing operations; changes go through apply().
    virtual model::Schematic& id_source() = 0;
    // The parts, including the project's blocks and subcircuit parts.
    virtual const library::ComponentLibrary& library() const = 0;
    virtual void apply(model::Edit edit) = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;

    // The project file; empty for a new project.
    virtual std::filesystem::path file() const = 0;
    // Folder that relative paths (firmware, memories) start from.
    virtual std::filesystem::path base_dir() const = 0;
    virtual bool modified() const = 0;
    // Project files (absolute paths). They throw Error: "io-error", or
    // "blocked" when unsaved changes would be lost and `discard` is false.
    virtual void new_project(bool discard) = 0;
    // The name an untitled project shows (the window's title) until it is saved.
    virtual void name_project(const std::string& name) { (void)name; }
    virtual std::vector<std::string> open_project(const std::filesystem::path& file, bool discard) = 0; // warnings
    virtual void save_project(const std::filesystem::path& file) = 0;

    virtual sim::Session& session() = 0;
    // Runs go through these, so that a window can show them.
    virtual bool start_run(sim::RunRequest request, std::string& reason)
    {
        return session().start(std::move(request), reason);
    }
    virtual void stop_run() { session().stop(); }
    // Lets time pass while a command waits for a run (a window keeps drawing
    // meanwhile); may throw Error "cancelled".
    virtual void wait(std::chrono::milliseconds time) { std::this_thread::sleep_for(time); }
    // People watch the workspace (the desktop window): runs keep the
    // project's speed instead of going as fast as possible.
    virtual bool watched() const { return false; }
};

// A workspace without a window: the project in memory, a plain undo history
// and an in-process simulation session.
class HeadlessWorkspace : public Workspace {
public:
    explicit HeadlessWorkspace(const library::ComponentLibrary& base);
    ~HeadlessWorkspace() override;

    const model::Schematic& schematic() const override { return schematic_; }
    model::Schematic& id_source() override { return schematic_; }
    const library::ComponentLibrary& library() const override { return *library_; }
    void apply(model::Edit edit) override;
    bool undo() override;
    bool redo() override;
    std::filesystem::path file() const override { return file_; }
    std::filesystem::path base_dir() const override;
    bool modified() const override { return modified_; }
    void new_project(bool discard) override;
    std::vector<std::string> open_project(const std::filesystem::path& file, bool discard) override;
    void save_project(const std::filesystem::path& file) override;
    sim::Session& session() override;

private:
    void replace(model::Schematic schematic, const std::filesystem::path& file);
    void refresh_library();

    const library::ComponentLibrary& base_;
    std::unique_ptr<library::ComponentLibrary> library_;
    model::Schematic schematic_;
    std::filesystem::path file_;
    std::vector<model::Edit> done_;
    std::vector<model::Edit> undone_;
    bool modified_ = false;
    std::unique_ptr<sim::SimulationSession> session_;
};

class Automation {
public:
    explicit Automation(Workspace& workspace);
    ~Automation();
    Automation(const Automation&) = delete;
    Automation& operator=(const Automation&) = delete;

    // Runs one request and returns its reply; never throws.
    json execute(const json& request);
    // Parses one line of JSON and runs it (code "bad-request" if it is not a JSON object).
    json execute_line(const std::string& line);

    // Every built-in command with its summary and arguments; "help" adds the extensions.
    static json help();

    // A command the host adds (the desktop window's view commands). `args`
    // describes the arguments as "help" lists them; `run` returns the result
    // or throws Error.
    struct Extension {
        std::string name;
        std::string summary;
        json args;
        std::function<json(const json& args)> run;
    };
    void extend(Extension extension);

    // Drops the current run, when something else started or ended one.
    void forget_run();

    struct Run; // the current run: its signals and messages

private:
    Workspace& workspace_;
    std::unique_ptr<Run> run_;
    std::vector<Extension> extensions_;
    std::filesystem::path directory_; // where relative paths start (hello's "directory")
};

} // namespace simnodus::automation
