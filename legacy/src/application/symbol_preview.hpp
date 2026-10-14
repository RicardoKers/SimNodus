// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"
#include "application/project_inspection.hpp"
#include "adapters/symbols/fixture_artwork.hpp"

namespace simnodus {
enum class SymbolPreviewStage { selection, resources, artwork };
struct SymbolPreviewError {
    SymbolPreviewStage stage;
    const char* code;
    std::size_t offset = 0;
    std::uint32_t system_code = 0;
};
struct DeclaredPreviewPin {
    std::string logical_pin, symbol_pin;
    bool operator==(const DeclaredPreviewPin&) const = default;
};
struct SymbolPreviewSelection {
    std::string component, symbol, asset;
    ResourceRequest request;
    std::vector<DeclaredPreviewPin> pin_map; // Complete owned declared map, sorted by logical ID.
};
using SymbolPreviewSelectionResult = std::variant<SymbolPreviewSelection, SymbolPreviewError>;
// Pure selection over a loader-owned validated graph. Only resistor in this slice.
// Returned metadata is owned; no resource I/O or global verification flags.
SymbolPreviewSelectionResult select_fixture_symbol(const ProjectGraph& graph, std::string_view component);
struct FixturePreviewPin {
    std::string logical_pin, symbol_pin;
    double x, y;
    bool operator==(const FixturePreviewPin&) const = default;
};
using FixturePreviewPins = std::array<FixturePreviewPin, 2>;
using FixturePreviewPinsResult = std::variant<FixturePreviewPins, SymbolPreviewError>;
// Pure declared bijection + closed fixture convention, not electrical/model truth.
// Owned rows are ordered by anchor ID, independently of source/label order.
FixturePreviewPinsResult inspect_fixture_pins(const ProjectGraph& graph, std::string_view component);
struct SymbolPreviewCapture {
    SymbolPreviewSelection selection;
    std::string requested_root; // Explicit request spelling, not a lease/trusted origin.
    ResourceSnapshots resources;
    symbols::FixtureArtwork artwork;
    std::optional<FixturePreviewPins> pins; // Unavailable for unknown descriptor/anchors.
};
using SymbolPreviewResult = std::variant<std::shared_ptr<const SymbolPreviewCapture>, SymbolPreviewError>;
// Explicit selected-symbol capture from an independently supplied root. Never
// captures other lock files, executes resources or changes the document graph.
SymbolPreviewResult capture_fixture_symbol(const ProjectGraph& graph,
    std::string_view component, const std::string& resource_root);
// Pure full-binding comparison; names, source offsets and applied values do not
// identify artwork. Used by both independent views, with no resource I/O.
bool fixture_capture_matches(const ProjectGraph& graph, std::string_view component,
    const SymbolPreviewCapture& capture);
struct OccurrenceArtworkCapture {
    ComponentOccurrenceView occurrence;
    std::shared_ptr<const SymbolPreviewCapture> symbol;
};
using OccurrenceArtworkResult = std::variant<std::shared_ptr<const OccurrenceArtworkCapture>, SymbolPreviewError>;
// Independent explicit one-file capture for an existing component ID path.
// No captured library Preview or inferred resource root is reused.
OccurrenceArtworkResult capture_fixture_occurrence(const ProjectGraph& graph,
    std::span<const std::string> path, const std::string& resource_root);
}
