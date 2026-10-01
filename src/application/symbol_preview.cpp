// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/symbol_preview.hpp"
#include <new>

namespace simnodus {
SymbolPreviewSelectionResult select_fixture_symbol(const ProjectGraph& graph, std::string_view component)
{
    try {
        if(component != "resistor") return SymbolPreviewError{SymbolPreviewStage::selection, "component"};
        const auto source = graph.source_map.find({"components", std::string(component)});
        if(source == graph.source_map.end()) return SymbolPreviewError{SymbolPreviewStage::selection, "component"};
        const auto& syntax = *graph.declaration->sources.lock.syntax;
        const auto field = [&syntax](std::size_t index, std::string_view key) {
            for(auto k = index + 1; k < syntax.tokens[index].next; k = syntax.tokens[k + 1].next)
                if(syntax.tokens[k].decoded == key) return k + 1;
            return syntax.tokens.size();
        };
        std::size_t object = 0;
        while(object < syntax.tokens.size() && syntax.tokens[object].begin != source->second.begin) ++object;
        const auto binding = field(object, "symbol");
        if(syntax.tokens[binding].kind == JsonKind::null)
            return SymbolPreviewError{SymbolPreviewStage::selection, "symbol", syntax.tokens[binding].begin};
        const auto symbol = syntax.tokens[field(binding, "definition")].decoded;
        const auto sources = field(0, "sources");
        const auto asset_token = field(field(sources, "symbols"), symbol);
        if(syntax.tokens[asset_token].kind == JsonKind::null)
            return SymbolPreviewError{SymbolPreviewStage::selection, "symbol", syntax.tokens[asset_token].begin};
        const auto asset = syntax.tokens[asset_token].decoded;
        const auto assets = field(sources, "assets");
        for(auto k = assets + 1; k < syntax.tokens[assets].next; k = syntax.tokens[k].next) {
            if(syntax.tokens[field(k, "id")].decoded != asset) continue;
            if(syntax.tokens[field(k, "kind")].decoded != "symbol-svg")
                return SymbolPreviewError{SymbolPreviewStage::selection, "asset", syntax.tokens[k].begin};
            const auto dependency = syntax.tokens[field(k, "dependency")].decoded;
            const auto resource = syntax.tokens[field(k, "resource")].decoded;
            for(const auto& request : graph.declaration->sources.lock.requests) {
                if(request.dependency != dependency || request.resource != resource) continue;
                if(request.bytes != 227 || request.sha256 != "94bfd7453b5604800ebd5870586d51ec369eca1b3a59b04b2ec0890eb98579d2")
                    return SymbolPreviewError{SymbolPreviewStage::selection, "artwork", syntax.tokens[k].begin};
                return SymbolPreviewSelection{std::string(component), symbol, asset, request};
            }
        }
        return SymbolPreviewError{SymbolPreviewStage::selection, "resource"};
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
        return std::make_shared<const SymbolPreviewCapture>(SymbolPreviewCapture{
            std::move(selection), resource_root, std::move(resources), *artwork});
    } catch(const std::bad_alloc&) { return SymbolPreviewError{stage, "memory"}; }
}
}
