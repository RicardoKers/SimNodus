// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Automation commands without the engine (SN-215): parts, editing, routing,
// checks, undo, files and error codes.
#include "test_support.hpp"

#include "automation/automation.hpp"
#include "automation/mcp.hpp"

#include <filesystem>
#include <string>

using namespace simnodus;
using automation::json;

namespace {

const library::ComponentLibrary& parts()
{
    static library::ComponentLibrary library = [] {
        library::ComponentLibrary l;
        l.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
        return l;
    }();
    return library;
}

struct Fixture {
    automation::HeadlessWorkspace workspace{parts()};
    automation::Automation automation{workspace};

    json call(const std::string& command, json args = json::object())
    {
        return automation.execute({{"command", command}, {"args", std::move(args)}});
    }
    json ok(const std::string& command, json args = json::object())
    {
        const auto reply = call(command, std::move(args));
        if (!reply.value("ok", false)) {
            std::printf("  %s failed: %s\n", command.c_str(), reply.dump().c_str());
        }
        return reply.value("ok", false) ? reply["result"] : json(nullptr);
    }
    std::string error(const std::string& command, json args = json::object())
    {
        const auto reply = call(command, std::move(args));
        return reply.value("ok", true) ? std::string() : reply["error"].value("code", "");
    }

    // A pulse source driving an RC low-pass, OUT labelled.
    void rc()
    {
        ok("component.add", {{"type", "vpulse"}, {"at", {0, 0}}, {"parameters", {{"f", "100"}}}});
        ok("component.add", {{"type", "resistor"}, {"at", {6, -6}}, {"rotation", 270}, {"parameters", {{"R", "1k"}}}});
        ok("component.add", {{"type", "capacitor"}, {"at", {12, 0}}, {"parameters", {{"C", "1u"}}}});
        ok("component.add", {{"type", "ground"}, {"attach", {{"to", "V1.-"}}}});
        ok("wire.connect", {{"from", "V1.+"}, {"to", "R1.1"}});
        ok("wire.connect", {{"from", "R1.2"}, {"to", "C1.1"}});
        ok("wire.connect", {{"from", "C1.2"}, {"to", "V1.-"}});
        ok("label.add", {{"name", "OUT"}, {"at", "C1.1"}});
    }
};

const json* find_net(const json& inspect, const std::string& name)
{
    for (const auto& n : inspect["nets"]) {
        if (n["name"] == name) {
            return &n;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("automation: hello, help and malformed requests")
{
    Fixture f;
    CHECK(f.ok("hello")["protocol"] == automation::protocol_version);
    CHECK(f.ok("hello")["window"] == false);
    const auto help = f.ok("help");
    REQUIRE(help.is_object());
    CHECK(help["commands"].size() > 25);
    for (const auto& c : help["commands"]) {
        CHECK(c["args"].is_object()); // every argument description parses as JSON
    }
    CHECK(f.automation.execute_line("not json")["error"]["code"] == "bad-request");
    CHECK(f.automation.execute_line("[1]")["error"]["code"] == "bad-request");
    CHECK(f.error("no.such") == "unknown-command");
    const auto reply = f.automation.execute_line(R"({"id": 7, "command": "hello"})");
    CHECK(reply["id"] == 7);
}

TEST_CASE("automation: parts are listed, searched and described")
{
    Fixture f;
    const auto all = f.ok("parts.list");
    CHECK(all["parts"].size() == parts().definitions().size());
    const auto found = f.ok("parts.list", {{"search", "capacitor"}});
    REQUIRE(!found["parts"].empty());
    const auto resistor = f.ok("parts.describe", {{"type", "resistor"}});
    REQUIRE(resistor.is_object());
    CHECK(resistor["pins"].size() == 2);
    CHECK(resistor["parameters"][0]["name"] == "R");
    CHECK(resistor["bounds"].size() == 4);
    CHECK(f.error("parts.describe", {{"type", "flux_capacitor"}}) == "not-found");
}

TEST_CASE("automation: a circuit built by commands has the expected nets and passes the checks")
{
    Fixture f;
    f.rc();
    const auto check = f.ok("circuit.check");
    CHECK(check["ok"] == true);
    CHECK(check["errors"] == 0);
    const auto inspect = f.ok("circuit.inspect");
    REQUIRE(inspect.is_object());
    CHECK(inspect["components"].size() == 4);
    const auto* out = find_net(inspect, "OUT");
    REQUIRE(out);
    CHECK((*out)["pins"].size() == 2); // R1.2 and C1.1
    const auto* ground = find_net(inspect, "0");
    REQUIRE(ground);
    CHECK((*ground)["pins"].size() == 3); // V1.-, C1.2, GND1
    CHECK(inspect["unconnected_pins"].empty());
    CHECK(inspect["dangling_wire_ends"].empty());
    const auto& r1 = inspect["components"][1];
    CHECK(r1["designator"] == "R1");
    CHECK(r1["parameters"]["R"] == "1k");
}

TEST_CASE("automation: wire.connect never joins a pin it passes")
{
    Fixture f;
    // R2 sits on the straight line between R1.2 and R3.1.
    f.ok("component.add", {{"type", "resistor"}, {"at", {0, 0}}, {"rotation", 270}});
    f.ok("component.add", {{"type", "resistor"}, {"at", {10, 0}}, {"rotation", 270}});
    f.ok("component.add", {{"type", "resistor"}, {"at", {20, 0}}, {"rotation", 270}});
    // A straight wire from R1.2 to R3.1 would run through both pins of R2.
    const auto wire = f.call("wire.connect", {{"from", "R1.2"}, {"to", "R3.1"}});
    CHECK(wire.value("ok", true) == false);
    CHECK(wire["error"]["code"] == "blocked");
    // Around it with a corner point.
    const auto routed = f.ok("wire.connect", {{"from", "R1.2"}, {"to", "R3.1"}, {"via", {{5, 4}, {15, 4}}}});
    REQUIRE(routed.is_object());
    const auto inspect = f.ok("circuit.inspect");
    const auto& components = inspect["components"];
    const auto net = routed["net"];
    CHECK(components[0]["pins"][1]["net"] == net); // R1.2
    CHECK(components[2]["pins"][0]["net"] == net); // R3.1
    CHECK(components[1]["pins"][0]["net"] != net); // R2 stays out
    CHECK(components[1]["pins"][1]["net"] != net);
}

TEST_CASE("automation: parameters are checked, undo and redo restore the circuit")
{
    Fixture f;
    f.ok("component.add", {{"type", "resistor"}, {"at", {0, 0}}});
    CHECK(f.error("component.set", {{"component", "R1"}, {"parameters", {{"R", "abc"}}}}) == "invalid-argument");
    CHECK(f.error("component.set", {{"component", "R1"}, {"parameters", {{"Q", "1"}}}}) == "not-found");
    CHECK(f.error("component.set", {{"component", "R9"}, {"parameters", {{"R", "1"}}}}) == "not-found");
    CHECK(f.error("wire.connect", {{"from", "R1.7"}, {"to", "R1.1"}}) == "not-found");
    CHECK(f.error("component.add", {{"type", "resistor"}}) == "invalid-argument");
    f.ok("component.set", {{"component", "R1"}, {"parameters", {{"R", "4.7k"}}}, {"rotation", 90}});
    auto r1 = f.workspace.schematic().component_by_designator("R1");
    REQUIRE(r1);
    CHECK(r1->parameters.at("R") == "4.7k");
    CHECK(r1->orientation.rotation == 90);
    CHECK(f.ok("edit.undo")["undone"] == true);
    r1 = f.workspace.schematic().component_by_designator("R1");
    REQUIRE(r1);
    CHECK(r1->orientation.rotation == 0);
    CHECK(f.ok("edit.redo")["redone"] == true);
    CHECK(f.workspace.schematic().component_by_designator("R1")->orientation.rotation == 90);
    f.ok("items.delete", {{"items", {"R1"}}});
    CHECK(f.workspace.schematic().components().empty());
}

TEST_CASE("automation: projects are saved and opened again")
{
    Fixture f;
    f.rc();
    f.ok("settings.set", {{"mode", "transient"}, {"transient_stop", "20ms"}});
    const auto folder = std::filesystem::temp_directory_path() / "simnodus_automation_test";
    std::filesystem::create_directories(folder);
    const auto file = folder / "rc.snx";
    const auto file_text = file.u8string();
    const std::string path(file_text.begin(), file_text.end());
    REQUIRE(f.ok("project.save", {{"path", path}}).is_object());
    CHECK(f.ok("project.info")["modified"] == false);
    Fixture g;
    const auto opened = g.ok("project.open", {{"path", path}});
    REQUIRE(opened.is_object());
    CHECK(opened["components"] == 4);
    CHECK(g.ok("settings.get")["transient_stop"] == 0.02);
    CHECK(g.error("project.open", {{"path", path + ".missing"}}) == "io-error");
    CHECK(g.error("sim.read") == "not-running");

    // Relative paths start from hello's "directory" (simnodus-cli connect sends its own).
    Fixture h;
    const auto hello = h.ok("hello", {{"directory", path.substr(0, path.size() - 7)}}); // without "/rc.snx"
    REQUIRE(hello.is_object());
    CHECK(hello.contains("directory"));
    const auto relative = h.ok("project.open", {{"path", "rc.snx"}});
    REQUIRE(relative.is_object());
    CHECK(relative["components"] == 4);
    REQUIRE(h.ok("project.save", {{"path", "copy.snx"}}).is_object());
    CHECK(std::filesystem::exists(folder / "copy.snx"));
    std::filesystem::remove_all(folder);
}

TEST_CASE("automation: hosts add commands of their own")
{
    Fixture f;
    int calls = 0;
    f.automation.extend({"view.ping", "Answers.", json{{"n", "a number"}}, [&](const json& args) {
                             ++calls;
                             if (!args.contains("n")) {
                                 throw automation::Error{"invalid-argument", "n is missing"};
                             }
                             return json{{"n", args["n"]}};
                         }});
    CHECK(f.ok("view.ping", {{"n", 3}})["n"] == 3);
    CHECK(f.error("view.ping") == "invalid-argument");
    CHECK(calls == 2);
    const auto help = f.ok("help");
    CHECK(help["commands"].back()["command"] == "view.ping");
    CHECK(automation::Automation::help()["commands"].back()["command"] != "view.ping");
    f.automation.forget_run();
    CHECK(f.error("sim.read") == "not-running");
}

TEST_CASE("automation: the MCP server lists the commands as tools and calls them")
{
    Fixture f;
    automation::McpServer server([&](const json& request) { return f.automation.execute(request); }, "test");
    const auto init = server.handle({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2025-03-26"}, {"capabilities", json::object()}}}});
    REQUIRE(init.has_value());
    CHECK((*init)["result"]["protocolVersion"] == "2025-03-26");
    CHECK((*init)["result"]["serverInfo"]["name"] == "simnodus");
    const auto newer = server.handle({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "initialize"},
        {"params", {{"protocolVersion", "2099-01-01"}}}});
    CHECK((*newer)["result"]["protocolVersion"] == automation::McpServer::protocol_versions().front());
    CHECK(!server.handle({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}).has_value());

    const auto list = server.handle({{"jsonrpc", "2.0"}, {"id", 3}, {"method", "tools/list"}});
    REQUIRE(list.has_value());
    const auto& tools = (*list)["result"]["tools"];
    CHECK(tools.size() == automation::Automation::help()["commands"].size());
    bool found = false;
    for (const auto& t : tools) {
        CHECK(t["name"].get<std::string>().find('.') == std::string::npos);
        CHECK(t["inputSchema"]["type"] == "object");
        found = found || (t["name"] == "component_add" && t["inputSchema"]["properties"].contains("attach"));
    }
    CHECK(found);

    const auto added = server.handle({{"jsonrpc", "2.0"}, {"id", 4}, {"method", "tools/call"},
        {"params", {{"name", "component_add"}, {"arguments", {{"type", "resistor"}, {"at", {0, 0}}}}}}});
    REQUIRE(added.has_value());
    CHECK((*added)["result"]["isError"] == false);
    CHECK(json::parse((*added)["result"]["content"][0]["text"].get<std::string>())["designator"] == "R1");
    CHECK(f.workspace.schematic().components().size() == 1);
    const auto failed = server.handle({{"jsonrpc", "2.0"}, {"id", 5}, {"method", "tools/call"},
        {"params", {{"name", "component_set"}, {"arguments", {{"component", "R9"}}}}}});
    CHECK((*failed)["result"]["isError"] == true);
    CHECK(json::parse((*failed)["result"]["content"][0]["text"].get<std::string>())["code"] == "not-found");

    CHECK((*server.handle_line("not json"))["error"]["code"] == -32700);
    CHECK((*server.handle({{"jsonrpc", "2.0"}, {"id", 6}, {"method", "resources/list"}}))["error"]["code"] == -32601);
    CHECK((*server.handle({{"jsonrpc", "2.0"}, {"id", 7}, {"method", "ping"}}))["result"].empty());
}
