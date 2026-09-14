// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/resource_links.hpp"
#include "application/captured_validation_internal.hpp"
#include <algorithm>
#include <map>
#include <new>
#include <set>

namespace simnodus {
namespace {
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
struct Asset { std::string kind; };
class Validator {
public:
    explicit Validator(std::shared_ptr<const DeclarationSyntax> source) : source_(std::move(source)) {}
    ResourceLinkDeclaration run()
    {
        const auto root = exact(0, {"format", "version", "topology", "lock", "assets", "symbols", "models"});
        require(text(root.at("format"), "version") == "simnodus-resource-links"
            && text(root.at("version"), "version") == "0.1", "version", 0);
        auto topology = detail::captured_bindings(source_, root.at("topology"));
        auto lock = detail::captured_lock(source_, root.at("lock"));
        std::set<std::pair<std::string, std::string>> files, targets;
        for(const auto& row : lock.requests) files.emplace(row.dependency, row.resource);
        const auto values = array(root.at("assets"));
        require(values.size() <= resource_max_files, "budget", root.at("assets"));
        std::map<std::string, Asset> assets;
        for(const auto token : values) {
            const auto value = exact(token, {"id", "kind", "dependency", "resource"});
            const auto id = identifier(value.at("id"));
            const auto dependency = identifier(value.at("dependency"));
            const auto resource = identifier(value.at("resource"));
            const auto kind = text(value.at("kind"), "kind");
            require(kind == "symbol-svg" || kind == "model-spice", "kind", value.at("kind"));
            require(assets.emplace(id, Asset{kind}).second, "id", value.at("id"));
            const auto target = std::make_pair(dependency, resource);
            require(files.contains(target) && targets.insert(target).second, "reference", token);
        }
        const auto topology_fields = object(root.at("topology"));
        std::size_t linked = 0, extra = assets.size();
        for(const bool model : {false, true}) {
            const auto collection = model ? "models" : "symbols";
            Fields descriptors;
            for(const auto token : array(topology_fields.at(collection)))
                descriptors.emplace(text(object(token).at("id"), "id"), token);
            const auto mappings = complete_map(root.at(collection), descriptors);
            extra += mappings.size();
            for(const auto& [id, target] : mappings) {
                if(source_->tokens[target].kind == JsonKind::null) continue;
                auto asset_token = target;
                if(model) {
                    const auto fields = exact(target, {"asset", "entrypoint", "terminal_map", "parameter_map"});
                    model_token(fields.at("entrypoint"));
                    const auto descriptor = object(descriptors.at(id));
                    for(const bool parameter : {false, true}) {
                        Fields interface;
                        for(const auto token : array(descriptor.at(parameter ? "parameters" : "terminals")))
                            interface.emplace(text(object(token).at("id"), "id"), token);
                        const auto map_index = fields.at(parameter ? "parameter_map" : "terminal_map");
                        const auto mapping = complete_map(map_index, interface);
                        std::set<std::string> names;
                        for(const auto& [source, destination] : mapping) {
                            (void)source;
                            auto name = model_token(destination);
                            for(auto& c : name) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
                            require(names.insert(name).second, "mapping", destination);
                        }
                        extra += mapping.size();
                    }
                    ++extra;
                    asset_token = fields.at("asset");
                }
                const auto asset = identifier(asset_token);
                const auto found = assets.find(asset);
                require(found != assets.end(), "reference", asset_token);
                require(found->second.kind == (model ? "model-spice" : "symbol-svg"), "kind", asset_token);
                ++linked;
            }
        }
        const auto& p = topology.parameters;
        require(p.topology.declared_entities + p.parameter_entries + topology.descriptor_entries + extra <= 4096, "budget", 0);
        return {std::move(topology), std::move(lock), linked, assets.size(), false};
    }
private:
    std::shared_ptr<const DeclarationSyntax> source_;
    void require(bool value, const char* code, Index index) const
    {
        if(!value) throw LockError{code, source_->tokens[index].begin};
    }
    Fields object(Index index, const char* code = "shape") const
    {
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::object, code, index);
        Fields result;
        for(auto child = index + 1; child < token.next; child = source_->tokens[child + 1].next)
            result.emplace(source_->tokens[child].decoded, child + 1);
        return result;
    }
    Fields exact(Index index, std::initializer_list<const char*> names) const
    {
        auto result = object(index);
        require(result.size() == names.size(), "shape", index);
        for(const auto name : names) require(result.contains(name), "shape", index);
        return result;
    }
    Fields complete_map(Index index, const Fields& expected) const
    {
        auto result = object(index, "mapping");
        require(result.size() == expected.size(), "mapping", index);
        for(const auto& [id, value] : result) { (void)value; require(expected.contains(id), "mapping", index); }
        return result;
    }
    std::vector<Index> array(Index index) const
    {
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::array, "shape", index);
        std::vector<Index> result;
        for(auto child = index + 1; child < token.next; child = source_->tokens[child].next) result.push_back(child);
        return result;
    }
    std::string text(Index index, const char* code) const
    {
        require(source_->tokens[index].kind == JsonKind::string, code, index);
        return source_->tokens[index].decoded;
    }
    std::string identifier(Index index) const
    {
        const auto value = text(index, "id");
        require(!value.empty() && value.size() <= 64 && value[0] >= 'a' && value[0] <= 'z'
            && std::all_of(value.begin(), value.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }), "id", index);
        return value;
    }
    std::string model_token(Index index) const
    {
        const auto value = text(index, "mapping");
        const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
        require(!value.empty() && value.size() <= 64 && letter(value[0])
            && std::all_of(value.begin(), value.end(), [&](char c) { return letter(c) || (c >= '0' && c <= '9'); }), "mapping", index);
        return value;
    }
};
}
ResourceLinkResult validate_resource_links(std::string_view bytes)
{
    try {
        const auto input = capture_declaration_syntax(bytes);
        if(const auto* error = std::get_if<IngressError>(&input))
            return LockError{error->code == IngressErrorCode::memory ? "memory" : "input", error->offset};
        return std::make_shared<const ResourceLinkDeclaration>(Validator(std::get<0>(input)).run());
    } catch(const TopologyError& error) { return LockError{topology_error_name(error.code), error.offset}; }
    catch(const ParameterError& error) { return LockError{error.code, error.offset}; }
    catch(const LockError& error) { return error; }
    catch(const std::bad_alloc&) { return LockError{"memory", 0}; }
}
}
