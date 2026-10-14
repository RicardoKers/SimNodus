// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_graph.hpp"
#include <new>

namespace simnodus {
namespace {
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
class Builder {
public:
    explicit Builder(std::shared_ptr<const ProjectDeclaration> declaration)
        : source_(*declaration->sources.lock.syntax)
    {
        result_.declaration = std::move(declaration);
    }
    ProjectGraph run()
    {
        const auto topology = object(object(object(0).at("sources")).at("topology"));
        result_.connectivity.root = text(topology.at("root"));
        remember({"root"}, topology.at("root"));
        for(const auto index : array(topology.at("components"))) {
            const auto fields = object(index);
            GraphComponent component{identity(fields), {}};
            const GraphSourceIdentity path{"components", component.identity.id};
            remember(path, index);
            for(const auto pin_index : array(fields.at("pins"))) {
                const auto pin = object(pin_index);
                component.pins.push_back({identity(pin), PinDomain::electrical});
                remember(append(path, {"pins", component.pins.back().identity.id}), pin_index);
            }
            result_.connectivity.components.push_back(std::move(component));
        }
        for(const auto index : array(topology.at("circuits"))) {
            const auto fields = object(index);
            GraphCircuit circuit{identity(fields), {}, {}, {}};
            const GraphSourceIdentity path{"circuits", circuit.identity.id};
            remember(path, index);
            for(const auto port_index : array(fields.at("ports"))) {
                const auto port = object(port_index);
                const auto direction = text(port.at("direction"));
                const auto typed = direction == "input" ? PortDirection::input
                    : direction == "output" ? PortDirection::output : PortDirection::bidirectional;
                circuit.ports.push_back({identity(port), PinDomain::electrical, typed});
                remember(append(path, {"ports", circuit.ports.back().identity.id}), port_index);
            }
            for(const auto instance_index : array(fields.at("instances"))) {
                const auto instance = object(instance_index);
                circuit.instances.push_back({identity(instance), text(instance.at("kind")) == "component"
                    ? InstanceKind::component : InstanceKind::circuit, text(instance.at("definition"))});
                remember(append(path, {"instances", circuit.instances.back().identity.id}), instance_index);
            }
            for(const auto net_index : array(fields.at("nets"))) {
                const auto fields_net = object(net_index);
                GraphNet net{identity(fields_net), {}};
                const auto net_path = append(path, {"nets", net.identity.id});
                remember(net_path, net_index);
                for(const auto terminal_index : array(fields_net.at("terminals"))) {
                    const auto terminal = object(terminal_index);
                    if(terminal.contains("port")) {
                        const auto port = text(terminal.at("port"));
                        net.terminals.emplace_back(LocalPortTerminal{port});
                        remember(append(net_path, {"terminals", "port", port}), terminal_index);
                    } else {
                        const auto instance = text(terminal.at("instance")), pin = text(terminal.at("terminal"));
                        net.terminals.emplace_back(InstanceTerminal{instance, pin});
                        remember(append(net_path, {"terminals", "instance", instance, pin}), terminal_index);
                    }
                }
                circuit.nets.push_back(std::move(net));
            }
            result_.connectivity.circuits.push_back(std::move(circuit));
        }
        return std::move(result_);
    }
private:
    // The only caller provides a successful complete project validation result.
    // Never run this builder over arbitrary caller-constructed tokens or graphs.
    const DeclarationSyntax& source_;
    ProjectGraph result_;
    Fields object(Index index) const
    {
        Fields result;
        for(auto child = index + 1; child < source_.tokens[index].next; child = source_.tokens[child + 1].next)
            result.emplace(source_.tokens[child].decoded, child + 1);
        return result;
    }
    std::vector<Index> array(Index index) const
    {
        std::vector<Index> result;
        for(auto child = index + 1; child < source_.tokens[index].next; child = source_.tokens[child].next) result.push_back(child);
        return result;
    }
    std::string text(Index index) const { return source_.tokens[index].decoded; }
    GraphIdentity identity(const Fields& fields) const { return {text(fields.at("id")), text(fields.at("name"))}; }
    static GraphSourceIdentity append(GraphSourceIdentity path, std::initializer_list<std::string> parts)
    {
        path.insert(path.end(), parts.begin(), parts.end());
        return path;
    }
    void remember(GraphSourceIdentity path, Index index)
    {
        const auto& token = source_.tokens[index];
        result_.source_map.emplace(std::move(path), GraphSourceSpan{token.begin, token.end});
    }
};
}
ProjectGraphResult load_project_graph(std::string_view bytes)
{
    try {
        const auto declaration = validate_project_declaration(bytes);
        if(const auto* error = std::get_if<LockError>(&declaration)) return *error;
        return std::make_shared<const ProjectGraph>(Builder(std::get<0>(declaration)).run());
    } catch(const std::bad_alloc&) { return LockError{"memory", 0}; }
}
}
