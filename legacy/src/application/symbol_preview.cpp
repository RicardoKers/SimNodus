// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/symbol_preview.hpp"
#include <new>
#include <algorithm>

namespace simnodus {
namespace {
std::size_t field(const DeclarationSyntax& syntax, std::size_t index, std::string_view key)
{
    for(auto k = index + 1; k < syntax.tokens[index].next; k = syntax.tokens[k + 1].next)
        if(syntax.tokens[k].decoded == key) return k + 1;
    return syntax.tokens.size();
}
}
SymbolPreviewSelectionResult select_fixture_symbol(const ProjectGraph& graph, std::string_view component)
{
    try {
        if(component != "resistor") return SymbolPreviewError{SymbolPreviewStage::selection, "component"};
        const auto source = graph.source_map.find({"components", std::string(component)});
        if(source == graph.source_map.end()) return SymbolPreviewError{SymbolPreviewStage::selection, "component"};
        const auto& syntax = *graph.declaration->sources.lock.syntax;
        const auto member = [&syntax](std::size_t index, std::string_view key) { return field(syntax, index, key); };
        std::size_t object = 0;
        while(object < syntax.tokens.size() && syntax.tokens[object].begin != source->second.begin) ++object;
        const auto binding = member(object, "symbol");
        if(syntax.tokens[binding].kind == JsonKind::null)
            return SymbolPreviewError{SymbolPreviewStage::selection, "symbol", syntax.tokens[binding].begin};
        const auto symbol = syntax.tokens[member(binding, "definition")].decoded;
        const auto mapping = member(binding, "pin_map");
        std::vector<DeclaredPreviewPin> pin_map;
        for(auto k = mapping + 1; k < syntax.tokens[mapping].next; k = syntax.tokens[k + 1].next)
            pin_map.push_back({syntax.tokens[k].decoded, syntax.tokens[k + 1].decoded});
        std::sort(pin_map.begin(), pin_map.end(), [](const auto& a, const auto& b) { return a.logical_pin < b.logical_pin; });
        const auto sources = member(0, "sources");
        const auto asset_token = member(member(sources, "symbols"), symbol);
        if(syntax.tokens[asset_token].kind == JsonKind::null)
            return SymbolPreviewError{SymbolPreviewStage::selection, "symbol", syntax.tokens[asset_token].begin};
        const auto asset = syntax.tokens[asset_token].decoded;
        const auto assets = member(sources, "assets");
        for(auto k = assets + 1; k < syntax.tokens[assets].next; k = syntax.tokens[k].next) {
            if(syntax.tokens[member(k, "id")].decoded != asset) continue;
            if(syntax.tokens[member(k, "kind")].decoded != "symbol-svg")
                return SymbolPreviewError{SymbolPreviewStage::selection, "asset", syntax.tokens[k].begin};
            const auto dependency = syntax.tokens[member(k, "dependency")].decoded;
            const auto resource = syntax.tokens[member(k, "resource")].decoded;
            for(const auto& request : graph.declaration->sources.lock.requests) {
                if(request.dependency != dependency || request.resource != resource) continue;
                if(request.bytes != 227 || request.sha256 != "94bfd7453b5604800ebd5870586d51ec369eca1b3a59b04b2ec0890eb98579d2")
                    return SymbolPreviewError{SymbolPreviewStage::selection, "artwork", syntax.tokens[k].begin};
                return SymbolPreviewSelection{std::string(component), symbol, asset, request, std::move(pin_map)};
            }
        }
        return SymbolPreviewError{SymbolPreviewStage::selection, "resource"};
    } catch(const std::bad_alloc&) { return SymbolPreviewError{SymbolPreviewStage::selection, "memory"}; }
}
FixturePreviewPinsResult inspect_fixture_pins(const ProjectGraph& graph, std::string_view component)
{
    try {
        const auto selected = select_fixture_symbol(graph, component);
        if(const auto* error = std::get_if<SymbolPreviewError>(&selected)) return *error;
        const auto& selection = std::get<0>(selected);
        FixturePreviewPins pins{};
        std::array<bool, 2> assigned{};
        for(const auto& binding : selection.pin_map) {
            const auto& anchor_id = binding.symbol_pin;
            const auto anchor = symbols::fixture_anchor(selection.symbol, anchor_id);
            if(!anchor) return SymbolPreviewError{SymbolPreviewStage::selection, "anchors"};
            const auto slot = anchor_id == "a" ? 0u : 1u;
            if(assigned[slot]) return SymbolPreviewError{SymbolPreviewStage::selection, "anchors"};
            pins[slot] = {binding.logical_pin, anchor_id, anchor->x, anchor->y};
            assigned[slot] = true;
        }
        if(!assigned[0] || !assigned[1]) return SymbolPreviewError{SymbolPreviewStage::selection, "anchors"};
        return pins;
    } catch(const std::bad_alloc&) { return SymbolPreviewError{SymbolPreviewStage::selection, "memory"}; }
}
SymbolPreviewResult capture_fixture_symbol(const ProjectGraph& graph,
    std::string_view component, const std::string& resource_root)
{
    auto stage = SymbolPreviewStage::selection;
    try {
        auto selected = select_fixture_symbol(graph, component);
        if(const auto* error = std::get_if<SymbolPreviewError>(&selected)) return *error;
        auto selection = std::get<0>(std::move(selected));
        stage = SymbolPreviewStage::resources;
        const auto verified = verify_local_resources(resource_root, std::span<const ResourceRequest>(&selection.request, 1));
        if(const auto* error = std::get_if<ResourceError>(&verified))
            return SymbolPreviewError{stage, resource_error_name(error->code), error->index, error->system_code};
        auto resources = std::get<0>(verified);
        stage = SymbolPreviewStage::artwork;
        const auto& data = resources->front().data;
        const auto artwork = symbols::recognize_fixture_artwork(
            std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
        if(!artwork) return SymbolPreviewError{stage, "artwork"};
        const auto inspected = inspect_fixture_pins(graph, component);
        std::optional<FixturePreviewPins> pins;
        if(const auto* known = std::get_if<FixturePreviewPins>(&inspected)) pins = *known;
        else if(std::string_view(std::get<SymbolPreviewError>(inspected).code) == "memory")
            return std::get<SymbolPreviewError>(inspected);
        return std::make_shared<const SymbolPreviewCapture>(SymbolPreviewCapture{
            std::move(selection), resource_root, std::move(resources), *artwork, std::move(pins)});
    } catch(const std::bad_alloc&) { return SymbolPreviewError{stage, "memory"}; }
}
bool fixture_capture_matches(const ProjectGraph& graph, std::string_view component,
    const SymbolPreviewCapture& capture)
{
    const auto selected = select_fixture_symbol(graph, component);
    const auto* current = std::get_if<SymbolPreviewSelection>(&selected);
    if(!current) return false;
    const auto& old = capture.selection;
    const auto inspected = inspect_fixture_pins(graph, component);
    std::optional<FixturePreviewPins> pins;
    if(const auto* known = std::get_if<FixturePreviewPins>(&inspected)) pins = *known;
    return current->component == old.component && current->symbol == old.symbol && current->asset == old.asset &&
        current->request.dependency == old.request.dependency && current->request.resource == old.request.resource &&
        current->request.path == old.request.path && current->request.bytes == old.request.bytes && current->request.sha256 == old.request.sha256 &&
        current->pin_map == old.pin_map && pins == capture.pins;
}
OccurrenceArtworkResult capture_fixture_occurrence(const ProjectGraph& graph,
    std::span<const std::string> path, const std::string& resource_root)
{
    try {
        auto occurrence = inspect_component_occurrence(graph, path);
        if(!occurrence) return SymbolPreviewError{SymbolPreviewStage::selection, "occurrence"};
        const auto symbol = capture_fixture_symbol(graph, occurrence->component, resource_root);
        if(const auto* error = std::get_if<SymbolPreviewError>(&symbol)) return *error;
        return std::make_shared<const OccurrenceArtworkCapture>(OccurrenceArtworkCapture{
            std::move(*occurrence), std::get<0>(symbol)});
    } catch(const std::bad_alloc&) { return SymbolPreviewError{SymbolPreviewStage::selection, "memory"}; }
}
}
