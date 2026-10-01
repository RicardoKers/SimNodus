// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"
#include "adapters/symbols/fixture_artwork.hpp"

namespace simnodus {
enum class SymbolPreviewStage { selection, resources, artwork };
struct SymbolPreviewError {
    SymbolPreviewStage stage;
    const char* code;
    std::size_t offset = 0;
    std::uint32_t system_code = 0;
};
struct SymbolPreviewSelection { std::string component, symbol, asset; ResourceRequest request; };
using SymbolPreviewSelectionResult = std::variant<SymbolPreviewSelection, SymbolPreviewError>;
// Pure selection over a loader-owned validated graph. Only resistor in this slice.
// Returned metadata is owned; no resource I/O or global verification flags.
SymbolPreviewSelectionResult select_fixture_symbol(const ProjectGraph& graph, std::string_view component);
struct SymbolPreviewCapture {
    SymbolPreviewSelection selection;
    std::string requested_root; // Explicit request spelling, not a lease/trusted origin.
    ResourceSnapshots resources;
    symbols::FixtureArtwork artwork;
};
using SymbolPreviewResult = std::variant<std::shared_ptr<const SymbolPreviewCapture>, SymbolPreviewError>;
// Explicit selected-symbol capture from an independently supplied root. Never
// captures other lock files, executes resources or changes the document graph.
SymbolPreviewResult capture_fixture_symbol(const ProjectGraph& graph,
    std::string_view component, const std::string& resource_root);
}
