// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_revision.hpp"
#include <new>

namespace simnodus {
namespace {
std::size_t field(const DeclarationSyntax& syntax, std::size_t object, std::string_view name)
{
    if(object >= syntax.tokens.size() || syntax.tokens[object].kind != JsonKind::object) return syntax.tokens.size();
    for(std::size_t key = object + 1; key < syntax.tokens[object].next; key = syntax.tokens[key + 1].next)
        if(syntax.tokens[key].decoded == name) return key + 1;
    return syntax.tokens.size();
}
std::size_t instance_object(const ProjectGraph& graph, std::string_view circuit, std::string_view instance)
{
    const auto& syntax = *graph.declaration->sources.lock.syntax;
    for(const auto& [path, span] : graph.source_map) {
        if(path.size() != 4 || path[0] != "circuits" || path[1] != circuit || path[2] != "instances" || path[3] != instance) continue;
        for(std::size_t i = 0; i < syntax.tokens.size(); ++i)
            if(syntax.tokens[i].begin == span.begin && syntax.tokens[i].kind == JsonKind::object) return i;
    }
    return syntax.tokens.size();
}
std::size_t resistance_value(const ProjectGraph& graph, std::string_view circuit, std::string_view instance)
{
    const auto& syntax = *graph.declaration->sources.lock.syntax;
    return field(syntax, field(syntax, field(syntax, instance_object(graph, circuit, instance), "overrides"), "resistance"), "value");
}
ProjectRevisionResult replace_string(const std::shared_ptr<const ProjectGraph>& base,
    std::size_t selected, std::string_view name, std::size_t limit)
{
    const auto& syntax = *base->declaration->sources.lock.syntax;
    if(selected >= syntax.tokens.size())
        return ProjectRevisionError{ProjectRevisionStage::revision, "selector", 0};
    const auto& token = syntax.tokens[selected];
    if(token.decoded == name) return base;
    // Full field validation stays in the unchanged project validator.
    if(name.size() > limit) return ProjectRevisionError{ProjectRevisionStage::revision, "value", token.begin};
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
        return replace_string(graph, field(*graph->declaration->sources.lock.syntax, 0, "name"), name, 320);
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
        const auto& syntax = *graph->declaration->sources.lock.syntax;
        return replace_string(graph, field(syntax, instance_object(*graph, circuit_id, instance_id), "name"), name, 320);
    } catch(const std::bad_alloc&) { return ProjectRevisionError{stage, "memory", 0}; }
}
std::optional<LiteralResistanceView> literal_resistance(const ProjectGraph& graph,
    std::string_view circuit_id, std::string_view instance_id)
{
    const auto& syntax = *graph.declaration->sources.lock.syntax;
    const auto object = instance_object(graph, circuit_id, instance_id);
    const auto binding = field(syntax, field(syntax, object, "overrides"), "resistance");
    const auto value = field(syntax, binding, "value"), unit = field(syntax, binding, "unit");
    if(value >= syntax.tokens.size() || unit >= syntax.tokens.size()) return {};
    const auto& spelling = syntax.tokens[unit].decoded;
    if(spelling != "ohm" && spelling != "kohm") return {};
    const auto kind = field(syntax, object, "kind"), target = field(syntax, object, "definition");
    if(kind >= syntax.tokens.size() || target >= syntax.tokens.size()) return {};
    const auto collection = syntax.tokens[kind].decoded == "circuit" ? "circuits" : "components";
    for(const auto& [path, span] : graph.source_map) {
        if(path.size() != 2 || path[0] != collection || path[1] != syntax.tokens[target].decoded) continue;
        for(std::size_t i = 0; i < syntax.tokens.size(); ++i) {
            if(syntax.tokens[i].begin != span.begin || syntax.tokens[i].kind != JsonKind::object) continue;
            const auto parameters = field(syntax, i, "parameters");
            if(parameters >= syntax.tokens.size()) return {};
            for(auto p = parameters + 1; p < syntax.tokens[parameters].next; p = syntax.tokens[p].next) {
                const auto id = field(syntax, p, "id");
                if(id >= syntax.tokens.size() || syntax.tokens[id].decoded != "resistance") continue;
                const auto low = field(syntax, p, "minimum"), high = field(syntax, p, "maximum"), base_unit = field(syntax, p, "unit");
                if(low >= syntax.tokens.size() || high >= syntax.tokens.size() || base_unit >= syntax.tokens.size() || syntax.tokens[base_unit].decoded != "ohm") return {};
                return LiteralResistanceView{syntax.tokens[value].decoded, spelling,
                    syntax.tokens[low].decoded, syntax.tokens[high].decoded, syntax.tokens[base_unit].decoded};
            }
        }
    }
    return {};
}
ProjectRevisionResult revise_resistance(std::string_view original, std::string_view circuit_id,
    std::string_view instance_id, std::string_view value)
{
    auto stage = ProjectRevisionStage::base;
    try {
        const auto base = load_project_graph(original);
        if(const auto* error = std::get_if<LockError>(&base)) return ProjectRevisionError{stage, error->code, error->offset};
        stage = ProjectRevisionStage::revision;
        const auto& graph = std::get<0>(base);
        if(circuit_id.size() > 64 || instance_id.size() > 64 || !literal_resistance(*graph, circuit_id, instance_id))
            return ProjectRevisionError{stage, "selector", 0};
        return replace_string(graph, resistance_value(*graph, circuit_id, instance_id), value, 64);
    } catch(const std::bad_alloc&) { return ProjectRevisionError{stage, "memory", 0}; }
}
}
