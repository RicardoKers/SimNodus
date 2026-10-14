// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/passive_interface.hpp"
#include "application/lock_digest_internal.hpp"
#include <map>
#include <new>

namespace simnodus {
namespace {
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
std::string folded(std::string value)
{
    for(auto& c : value) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}
class Matcher {
public:
    explicit Matcher(std::shared_ptr<const ResourceLinkDeclaration> declaration)
        : syntax_(*declaration->lock.syntax) { result_.declaration = std::move(declaration); }
    PassiveInterface run(std::string_view id, std::shared_ptr<const spice::PassiveSource> source)
    {
        result_.source = std::move(source);
        const auto root = object(0), mappings = object(root.at("models"));
        const auto selected = mappings.find(std::string(id));
        require(selected != mappings.end(), "descriptor", 0);
        const auto binding_index = selected->second;
        require(syntax_.tokens[binding_index].kind != JsonKind::null, "absent", binding_index);
        const auto binding = object(binding_index);
        const auto topology = object(root.at("topology"));
        const auto descriptor_index = find_id(topology.at("models"), id);
        const auto descriptor = object(descriptor_index);
        const auto asset_id = text(binding.at("asset"));
        const auto asset = object(find_id(root.at("assets"), asset_id));
        const auto dependency = text(asset.at("dependency")), resource = text(asset.at("resource"));
        const ResourceRequest* request = nullptr;
        for(const auto& row : result_.declaration->lock.requests)
            if(row.dependency == dependency && row.resource == resource) request = &row;
        require(request != nullptr, "resource", binding_index);
        require(result_.source->bytes.size() == request->bytes, "size", binding_index);
        require(detail::lock_sha256(result_.source->bytes) == request->sha256, "hash", binding_index);
        const auto entrypoint = folded(text(binding.at("entrypoint")));
        Index model_index = 0;
        for(; model_index < result_.source->subcircuits.size(); ++model_index)
            if(folded(result_.source->subcircuits[model_index].name) == entrypoint) break;
        require(model_index < result_.source->subcircuits.size(), "entrypoint", binding.at("entrypoint"));
        const auto& model = result_.source->subcircuits[model_index];
        const auto terminals = object(binding.at("terminal_map")), parameters = object(binding.at("parameter_map"));
        require(terminals.size() == 2 && parameters.size() == 1, "arity", binding_index);
        for(Index position = 0; position < 2; ++position) {
            for(const auto& [terminal, token] : terminals)
                if(folded(text(token)) == folded(model.terminals[position])) result_.terminal_ids[position] = terminal;
            require(!result_.terminal_ids[position].empty(), "terminal", binding.at("terminal_map"));
        }
        const auto& [parameter, mapped] = *parameters.begin();
        require(folded(text(mapped)) == folded(model.parameter), "parameter", mapped);
        const auto parameter_fields = object(find_id(descriptor.at("parameters"), parameter));
        const auto unit = text(parameter_fields.at("unit"));
        require(unit == (model.kind == spice::PassiveKind::resistor ? "ohm" : "F"), "unit", parameter_fields.at("unit"));
        result_.model_index = model_index;
        result_.descriptor_offset = syntax_.tokens[descriptor_index].begin;
        result_.binding_offset = syntax_.tokens[binding_index].begin;
        result_.descriptor = id; result_.asset = asset_id;
        result_.dependency = dependency; result_.resource = resource;
        result_.parameter_id = parameter; result_.unit = unit;
        result_.minimum = text(parameter_fields.at("minimum"));
        result_.maximum = text(parameter_fields.at("maximum"));
        return std::move(result_);
    }
private:
    const DeclarationSyntax& syntax_;
    PassiveInterface result_{};
    void require(bool condition, const char* code, Index index) const
    {
        if(!condition) throw PassiveInterfaceError{PassiveInterfaceStage::correspondence, code, syntax_.tokens[index].begin};
    }
    Fields object(Index index) const
    {
        Fields result;
        for(auto child = index + 1; child < syntax_.tokens[index].next; child = syntax_.tokens[child + 1].next)
            result.emplace(syntax_.tokens[child].decoded, child + 1);
        return result;
    }
    std::string text(Index index) const { return syntax_.tokens[index].decoded; }
    Index find_id(Index array, std::string_view id) const
    {
        for(auto child = array + 1; child < syntax_.tokens[array].next; child = syntax_.tokens[child].next)
            if(text(object(child).at("id")) == id) return child;
        throw PassiveInterfaceError{PassiveInterfaceStage::correspondence, "reference", syntax_.tokens[array].begin};
    }
};
}
PassiveInterfaceResult match_passive_interface(std::string_view bytes, std::string_view id, std::string_view source)
{
    try {
        if(id.empty() || id.size() > 64) return PassiveInterfaceError{PassiveInterfaceStage::selection, "descriptor", 0};
        const auto declaration = validate_resource_links(bytes);
        if(const auto* error = std::get_if<LockError>(&declaration))
            return PassiveInterfaceError{PassiveInterfaceStage::declaration, error->code, error->offset};
        const auto parsed = spice::inspect_passive_source(source);
        if(const auto* error = std::get_if<spice::PassiveSourceError>(&parsed))
            return PassiveInterfaceError{PassiveInterfaceStage::source, error->code, error->offset};
        return std::make_shared<const PassiveInterface>(Matcher(std::get<0>(declaration)).run(id, std::get<0>(parsed)));
    } catch(const PassiveInterfaceError& error) { return error; }
    catch(const std::bad_alloc&) { return PassiveInterfaceError{PassiveInterfaceStage::correspondence, "memory", 0}; }
}
}
