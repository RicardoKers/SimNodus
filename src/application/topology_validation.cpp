// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/topology_validation.hpp"
#include <algorithm>
#include <map>
#include <new>
#include <set>
#include <utility>

namespace simnodus {
namespace {
using Code = TopologyErrorCode;
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
using Ids = std::set<std::string>;
struct Definition {
    Index token;
    Fields fields;
    Ids terminals;
    std::vector<std::string> children;
    std::size_t instances = 0, depth = 0, expanded = 0;
    bool active = false;
};
class Validator {
public:
    explicit Validator(std::shared_ptr<const DeclarationSyntax> source) : source_(std::move(source)) {}
    TopologyDeclaration run()
    {
        const auto root = fields(0, {"format", "version", "root", "components", "circuits"});
        require(equal(root.at("format"), "simnodus-topology") && equal(root.at("version"), "0.1"), Code::version, 0);
        const auto root_id = id(root.at("root"));
        Ids definitions;
        for(const bool component : {true, false}) {
            auto& catalog = component ? components_ : circuits_;
            for(const auto index : array(root.at(component ? "components" : "circuits"), component ? 0 : 1)) {
                auto record = component ? fields(index, {"id", "name", "pins"}) : fields(index, {"id", "name", "ports", "instances", "nets"});
                const auto name = register_id(record, definitions, index);
                catalog.emplace(name, Definition{index, std::move(record), {}, {}, 0, 0, 0, false});
            }
        }
        require(circuits_.contains(root_id), Code::reference, root.at("root"));
        for(const bool component : {true, false}) {
            for(auto& [name, definition] : component ? components_ : circuits_) {
                (void)name;
                for(const auto index : array(definition.fields.at(component ? "pins" : "ports"), component ? 1 : 0)) {
                    const auto record = component ? fields(index, {"id", "name", "domain"}) : fields(index, {"id", "name", "domain", "direction"});
                    register_id(record, definition.terminals, index);
                    require(equal(record.at("domain"), "electrical"), Code::value, index);
                    if(!component) require(equal(record.at("direction"), "input") || equal(record.at("direction"), "output")
                        || equal(record.at("direction"), "bidirectional"), Code::value, index);
                }
            }
        }
        for(auto& [name, definition] : circuits_) {
            (void)name;
            auto scope = definition.terminals;
            std::map<std::string, const Definition*> instances;
            for(const auto index : array(definition.fields.at("instances"))) {
                const auto record = fields(index, {"id", "name", "kind", "definition"});
                const auto instance_id = register_id(record, scope, index);
                const bool component = equal(record.at("kind"), "component");
                require(component || equal(record.at("kind"), "circuit"), Code::value, index);
                const auto target = id(record.at("definition"));
                const auto& catalog = component ? components_ : circuits_;
                const auto found = catalog.find(target);
                require(found != catalog.end(), Code::reference, record.at("definition"));
                instances.emplace(instance_id, &found->second);
                if(!component) definition.children.push_back(target);
            }
            definition.instances = instances.size();
            std::set<std::pair<std::string, std::string>> connected;
            for(const auto index : array(definition.fields.at("nets"))) {
                const auto record = fields(index, {"id", "name", "terminals"});
                register_id(record, scope, index);
                for(const auto terminal : array(record.at("terminals"), 2)) {
                    std::pair<std::string, std::string> identity;
                    const auto entries = object(terminal);
                    if(entries.size() == 1 && entries.contains("port")) {
                        identity.second = id(entries.at("port"));
                        require(definition.terminals.contains(identity.second), Code::reference, terminal);
                    } else {
                        const auto item = fields(terminal, {"instance", "terminal"});
                        identity.first = id(item.at("instance"));
                        identity.second = id(item.at("terminal"));
                        const auto found = instances.find(identity.first);
                        require(found != instances.end(), Code::reference, terminal);
                        require(found->second->terminals.contains(identity.second), Code::reference, terminal);
                    }
                    require(connected.insert(identity).second, Code::connection, terminal);
                }
            }
        }
        for(auto& [name, definition] : circuits_) { (void)name; walk(definition, 0); }
        const auto& selected = circuits_.at(root_id);
        return {source_, root_id, count_, selected.depth, selected.expanded};
    }
private:
    std::shared_ptr<const DeclarationSyntax> source_;
    std::map<std::string, Definition> components_, circuits_;
    std::size_t count_ = 0;
    void require(bool condition, Code code, Index index) const
    {
        if(!condition) throw TopologyError{code, source_->tokens[index].begin};
    }
    Fields object(Index index) const
    {
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::object, Code::shape, index);
        Fields result;
        for(auto child = index + 1; child < token.next; child = source_->tokens[child + 1].next)
            result.emplace(source_->tokens[child].decoded, child + 1);
        return result;
    }
    Fields fields(Index index, std::initializer_list<const char*> expected) const
    {
        auto result = object(index);
        require(result.size() == expected.size(), Code::shape, index);
        for(const auto name : expected) require(result.contains(name), Code::shape, index);
        return result;
    }
    std::vector<Index> array(Index index, std::size_t minimum = 0) const
    {
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::array, Code::shape, index);
        std::vector<Index> result;
        for(auto child = index + 1; child < token.next; child = source_->tokens[child].next) result.push_back(child);
        require(result.size() >= minimum, Code::shape, index);
        return result;
    }
    bool equal(Index index, std::string_view value) const
    {
        const auto& token = source_->tokens[index];
        return token.kind == JsonKind::string && token.decoded == value;
    }
    std::string id(Index index) const
    {
        const auto& token = source_->tokens[index];
        const auto& text = token.decoded;
        require(token.kind == JsonKind::string && !text.empty() && text.size() <= 64, Code::id, index);
        require(text[0] >= 'a' && text[0] <= 'z', Code::id, index);
        for(const auto c : text) require((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-', Code::id, index);
        return text;
    }
    std::string register_id(const Fields& record, Ids& scope, Index index)
    {
        const auto name = id(record.at("id"));
        const auto& token = source_->tokens[record.at("name")];
        require(token.kind == JsonKind::string, Code::value, record.at("name"));
        std::size_t length = 0;
        unsigned int scalar = 0;
        // The ingress has already rejected invalid/overlong UTF-8 and surrogates.
        for(const auto byte : token.decoded) {
            const auto c = static_cast<unsigned char>(byte);
            if((c & 0xc0) != 0x80) {
                if(length) require(scalar >= 32 && !(scalar >= 127 && scalar <= 159), Code::value, record.at("name"));
                ++length;
                scalar = c < 128 ? c : c < 224 ? c & 31 : c < 240 ? c & 15 : c & 7;
            } else scalar = (scalar << 6) | (c & 63);
        }
        require(length > 0 && length <= 80 && scalar >= 32 && !(scalar >= 127 && scalar <= 159), Code::value, record.at("name"));
        require(scope.insert(name).second, Code::id, record.at("id"));
        require(++count_ <= 4096, Code::budget, index);
        return name;
    }
    void walk(Definition& definition, std::size_t ancestors)
    {
        require(!definition.active && ancestors < 8, Code::hierarchy, definition.token);
        if(definition.depth) {
            require(ancestors + definition.depth <= 8, Code::hierarchy, definition.token);
            return;
        }
        definition.active = true;
        std::size_t depth = 1, expanded = definition.instances;
        for(const auto& child : definition.children) {
            auto& target = circuits_.at(child);
            walk(target, ancestors + 1);
            depth = std::max(depth, target.depth + 1);
            expanded += target.expanded;
            require(expanded <= 16384, Code::hierarchy, definition.token);
        }
        definition.active = false;
        definition.depth = depth;
        definition.expanded = expanded;
    }
};
}
TopologyResult validate_topology_declaration(std::string_view bytes)
{
    try {
        const auto input = capture_declaration_syntax(bytes);
        if(const auto* error = std::get_if<IngressError>(&input))
            return TopologyError{error->code == IngressErrorCode::memory ? Code::memory : Code::input, error->offset};
        return Validator(std::get<0>(input)).run();
    } catch(const TopologyError& error) { return error; }
    catch(const std::bad_alloc&) { return TopologyError{Code::memory, 0}; }
}
const char* topology_error_name(TopologyErrorCode code)
{
    switch(code) {
    case Code::input: return "input";
    case Code::shape: return "shape";
    case Code::version: return "version";
    case Code::id: return "id";
    case Code::value: return "value";
    case Code::reference: return "reference";
    case Code::connection: return "connection";
    case Code::hierarchy: return "hierarchy";
    case Code::budget: return "budget";
    case Code::memory: return "memory";
    }
    return "unknown";
}
}
