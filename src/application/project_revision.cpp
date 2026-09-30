// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_revision.hpp"
#include <algorithm>
#include <new>

namespace simnodus {
namespace {
std::size_t name_field(const DeclarationSyntax& syntax, std::size_t object)
{
    // Both callers select an object from the fully validated source graph.
    for(std::size_t key = object + 1; key < syntax.tokens[object].next; key = syntax.tokens[key + 1].next)
        if(syntax.tokens[key].decoded == "name") return key + 1;
    return syntax.tokens.size();
}
ProjectRevisionResult replace_name(const std::shared_ptr<const ProjectGraph>& base,
    std::size_t selected, std::string_view name)
{
    const auto& syntax = *base->declaration->sources.lock.syntax;
    if(selected >= syntax.tokens.size())
        return ProjectRevisionError{ProjectRevisionStage::revision, "selector", 0};
    const auto& token = syntax.tokens[selected];
    if(token.decoded == name) return base;
    // Full scalar/control validation stays in the unchanged validator.
    if(name.size() > 320) return ProjectRevisionError{ProjectRevisionStage::revision, "value", token.begin};
    std::string encoded = "\"";
    for(unsigned char c : name) {
        if(c == '"' || c == '\\') { encoded += '\\'; encoded += static_cast<char>(c); }
        else if(c < 32) {
            encoded += "\\u00";
            encoded += "0123456789abcdef"[c >> 4];
            encoded += "0123456789abcdef"[c & 15];
        } else encoded += static_cast<char>(c);
    }
    encoded += '"';
    const auto retained = syntax.bytes.size() - (token.end - token.begin);
    if(encoded.size() > declaration_max_bytes - retained)
        return ProjectRevisionError{ProjectRevisionStage::revision, "bytes", 0};
    auto revised = syntax.bytes.substr(0, token.begin);
    revised += encoded;
    revised.append(syntax.bytes, token.end, std::string::npos);
    const auto result = load_project_graph(revised);
    if(const auto* error = std::get_if<LockError>(&result))
        return ProjectRevisionError{ProjectRevisionStage::revision, error->code, error->offset};
    return std::get<0>(result);
}
}
ProjectRevisionResult rename_project(std::string_view original, std::string_view name)
{
    auto stage = ProjectRevisionStage::base;
    try {
        const auto base = load_project_graph(original);
        if(const auto* error = std::get_if<LockError>(&base))
            return ProjectRevisionError{stage, error->code, error->offset};
        stage = ProjectRevisionStage::revision;
        const auto& graph = std::get<0>(base);
        return replace_name(graph, name_field(*graph->declaration->sources.lock.syntax, 0), name);
    } catch(const std::bad_alloc&) { return ProjectRevisionError{stage, "memory", 0}; }
}
ProjectRevisionResult rename_instance(std::string_view original, std::string_view circuit_id,
    std::string_view instance_id, std::string_view name)
{
    auto stage = ProjectRevisionStage::base;
    try {
        const auto base = load_project_graph(original);
        if(const auto* error = std::get_if<LockError>(&base))
            return ProjectRevisionError{stage, error->code, error->offset};
        stage = ProjectRevisionStage::revision;
        if(circuit_id.size() > 64 || instance_id.size() > 64)
            return ProjectRevisionError{stage, "selector", 0};
        const auto& graph = std::get<0>(base);
        const auto found = graph->source_map.find({"circuits", std::string(circuit_id), "instances", std::string(instance_id)});
        if(found == graph->source_map.end()) return ProjectRevisionError{stage, "selector", 0};
        const auto& syntax = *graph->declaration->sources.lock.syntax;
        const auto token = std::find_if(syntax.tokens.begin(), syntax.tokens.end(), [&](const auto& value) {
            return value.begin == found->second.begin && value.kind == JsonKind::object;
        });
        if(token == syntax.tokens.end()) return ProjectRevisionError{stage, "selector", 0};
        const auto object = static_cast<std::size_t>(token - syntax.tokens.begin());
        return replace_name(graph, name_field(syntax, object), name);
    } catch(const std::bad_alloc&) { return ProjectRevisionError{stage, "memory", 0}; }
}
}
