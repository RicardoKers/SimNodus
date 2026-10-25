// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcp.hpp"

#include <algorithm>

namespace simnodus::automation {

namespace {

using json = McpServer::json;

json error_response(const json& id, int code, const std::string& message)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

std::string tool_name(std::string command)
{
    std::replace(command.begin(), command.end(), '.', '_');
    return command;
}

const char* instructions =
    "SimNodus simulates circuits and STM32 Blue Pill boards. Build circuits with component_add, wire_connect and "
    "label_add, check them with circuit_check, simulate with sim_run, and save with project_save. Grid points are "
    "integer [x, y] with y growing downwards; components are named by designator (\"R1\"), pins as DESIGNATOR.PIN "
    "(\"R1.2\"); quantities accept engineering text (\"4.7k\", \"10m\"). Call parts_list and parts_describe before "
    "placing parts. When hello reports \"window\": true, edits appear in the open SimNodus window; view_pace and "
    "view_caption help recorded tutorials.";

} // namespace

McpServer::McpServer(Execute execute, std::string version) : execute_(std::move(execute)), version_(std::move(version))
{
}

const std::vector<std::string>& McpServer::protocol_versions()
{
    static const std::vector<std::string> versions{"2025-06-18", "2025-03-26", "2024-11-05"};
    return versions;
}

std::optional<json> McpServer::handle_line(const std::string& line)
{
    json message;
    try {
        message = json::parse(line);
    } catch (const json::exception&) {
        return error_response(nullptr, -32700, "Parse error: the line is not JSON.");
    }
    return handle(message);
}

std::optional<json> McpServer::handle(const json& message)
{
    if (!message.is_object() || message.value("jsonrpc", "") != "2.0" || !message.contains("method") ||
        !message["method"].is_string()) {
        if (message.is_object() && !message.contains("method") && message.contains("id")) {
            return std::nullopt; // a response to a request of ours; this server sends none
        }
        return error_response(message.is_object() ? message.value("id", json(nullptr)) : json(nullptr), -32600,
            "Invalid request: expected a JSON-RPC 2.0 object with a method.");
    }
    const auto method = message["method"].get<std::string>();
    if (!message.contains("id")) {
        return std::nullopt; // notifications (initialized, cancelled) need no answer
    }
    const json id = message["id"];
    const json params = message.value("params", json::object());
    try {
        json result;
        if (method == "initialize") {
            const auto asked = params.is_object() ? params.value("protocolVersion", "") : std::string();
            const auto& known = protocol_versions();
            const bool supported = std::find(known.begin(), known.end(), asked) != known.end();
            result = {{"protocolVersion", supported ? asked : known.front()},
                {"capabilities", {{"tools", {{"listChanged", false}}}}},
                {"serverInfo", {{"name", "simnodus"}, {"title", "SimNodus"}, {"version", version_}}},
                {"instructions", instructions}};
        } else if (method == "ping") {
            result = json::object();
        } else if (method == "tools/list") {
            result = {{"tools", tools()}};
        } else if (method == "tools/call") {
            result = call(params);
        } else {
            return error_response(id, -32601, "Method not found: " + method);
        }
        return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    } catch (const json::exception& e) {
        return error_response(id, -32602, std::string("Invalid params: ") + e.what());
    }
}

json McpServer::tools()
{
    const auto reply = execute_({{"command", "help"}});
    json list = json::array();
    if (!reply.value("ok", false)) {
        return list;
    }
    commands_.clear();
    for (const auto& c : reply["result"]["commands"]) {
        const auto command = c["command"].get<std::string>();
        const auto name = tool_name(command);
        commands_[name] = command;
        json properties = json::object();
        for (const auto& [arg, text] : c["args"].items()) {
            properties[arg] = {{"description", text.is_string() ? text.get<std::string>() : text.dump()}};
        }
        list.push_back({{"name", name}, {"description", c["summary"].get<std::string>()},
            {"inputSchema", {{"type", "object"}, {"properties", properties}}}});
    }
    return list;
}

json McpServer::call(const json& params)
{
    const auto name = params.at("name").get<std::string>();
    if (commands_.empty()) {
        tools();
    }
    const auto it = commands_.find(name);
    // Unknown names still go through, so that the command reports them.
    const std::string command = it != commands_.end() ? it->second : name;
    json args = params.value("arguments", json::object());
    if (args.is_null()) {
        args = json::object();
    }
    const auto reply = execute_({{"command", command}, {"args", args}});
    const bool ok = reply.value("ok", false);
    const json body = ok ? reply.value("result", json::object()) : reply.value("error", json::object());
    return {{"content", json::array({{{"type", "text"},
                            {"text", body.dump(-1, ' ', false, json::error_handler_t::replace)}}})},
        {"isError", !ok}};
}

} // namespace simnodus::automation
