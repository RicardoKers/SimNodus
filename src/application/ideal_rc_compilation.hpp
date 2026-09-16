// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/connectivity_compilation.hpp"
#include "application/passive_numeric.hpp"

namespace simnodus {
enum class IdealRcProfile { unspecified, e01_ideal_rc };
struct IdealRcRequest {
    IdealRcProfile profile = IdealRcProfile::unspecified;
    std::string reference_port, drive_port, output_port;
};
struct IdealRcElementSource {
    OccurrencePath path;
    std::size_t netlist_line, project_parameter_offset;
    GraphSourceSpan project_instance;
    std::string dependency, resource;
    GraphSourceSpan model_source;
};
struct IdealRcCompilation {
    std::shared_ptr<const ConnectivityCompilation> connectivity;
    ResourceSnapshots resources;
    std::vector<std::shared_ptr<const PassiveNumericBinding>> bindings;
    IdealRcRequest request;
    std::string netlist;
    std::vector<IdealRcElementSource> elements;
    std::map<std::string, ConnectivityGroup> nodes;
};
enum class IdealRcStage { declaration, request, resources, binding, profile };
struct IdealRcError { IdealRcStage stage; const char* code; std::size_t offset; std::uint32_t system = 0; const char* coordinate = "project"; };
using IdealRcResult = std::variant<std::shared_ptr<const IdealRcCompilation>, IdealRcError>;
// Explicit Windows/NTFS compile operation. Verify/capture inventory once; consume
// owned bytes only. Never load an engine, execute, render, download or reopen paths.
IdealRcResult compile_ideal_rc(std::string_view project, const std::string& root,
    const IdealRcRequest& request);
}
