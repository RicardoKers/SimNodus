// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The automation commands as an MCP server (SN-217, ADR 0104): JSON-RPC 2.0
// messages, one per line on standard input and output ("stdio" transport).
// Every command becomes a tool named like the command with '_' for '.'
// ("component.add" -> "component_add"); a call runs the command and returns
// its JSON result as text, with isError set when the command failed. The
// server only translates: commands run through `execute`, in this process or
// in the SimNodus window.
#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::automation {

class McpServer {
public:
    using json = nlohmann::json;
    // Runs one automation request ({"command", "args"}) and returns its reply.
    using Execute = std::function<json(const json& request)>;

    McpServer(Execute execute, std::string version);

    // Handles one message; returns the response, or nothing for a notification.
    std::optional<json> handle(const json& message);
    // Parses one line first (a parse error gets a JSON-RPC error response).
    std::optional<json> handle_line(const std::string& line);

    // MCP revisions this server speaks; the newest is offered by default.
    static const std::vector<std::string>& protocol_versions();

private:
    json tools();
    json call(const json& params);

    Execute execute_;
    std::string version_;
    std::map<std::string, std::string> commands_; // tool name -> command
};

} // namespace simnodus::automation
