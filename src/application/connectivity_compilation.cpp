// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/connectivity_compilation.hpp"
#include <algorithm>
#include <new>

namespace simnodus {
namespace {
class Builder {
public:
    explicit Builder(std::shared_ptr<const ProjectGraph> source)
    {
        result_.source = std::move(source);
        for(const auto& component : result_.source->connectivity.components) components_.emplace(component.identity.id, &component);
        for(const auto& circuit : result_.source->connectivity.circuits) circuits_.emplace(circuit.identity.id, &circuit);
    }
    ConnectivityCompilation run()
    {
        const auto& root = result_.source->connectivity.root;
        occurrence({root}, root, InstanceKind::circuit, result_.source->source_map.at({"root"}));
        expand(*circuits_.at(root), {root});
        std::map<std::size_t, ConnectivityGroup> groups;
        for(const auto& [terminal, index] : nodes_) groups[find(index)].terminals.push_back(terminal);
        for(const auto& [net, index] : nets_) groups[find(index)].source_nets.push_back(net);
        for(auto& [index, group] : groups) { (void)index; result_.connections.push_back(std::move(group)); }
        std::sort(result_.connections.begin(), result_.connections.end(), [](const auto& a, const auto& b) {
            return a.terminals.front() < b.terminals.front();
        });
        std::sort(result_.occurrences.begin(), result_.occurrences.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
        return std::move(result_);
    }
private:
    ConnectivityCompilation result_;
    std::map<std::string, const GraphComponent*> components_;
    std::map<std::string, const GraphCircuit*> circuits_;
    std::map<CompiledTerminal, std::size_t> nodes_;
    std::map<CompiledNet, std::size_t> nets_;
    std::vector<std::size_t> parents_, sizes_;
    std::size_t records_ = 0;
    void budget(GraphSourceSpan span)
    {
        if(++records_ > connectivity_max_records) throw LockError{"expansion-budget", span.begin};
    }
    void occurrence(const OccurrencePath& path, const std::string& definition, InstanceKind kind, GraphSourceSpan span)
    {
        budget(span);
        result_.occurrences.push_back({path, definition, kind});
        result_.occurrence_sources.emplace(path, span);
    }
    void terminal(const OccurrencePath& path, const std::string& id, const GraphSourceIdentity& source)
    {
        const auto span = result_.source->source_map.at(source);
        budget(span);
        const CompiledTerminal key{path, id};
        const auto index = parents_.size();
        nodes_.emplace(key, index);
        parents_.push_back(index); sizes_.push_back(1);
        result_.terminal_sources.emplace(key, span);
    }
    std::size_t find(std::size_t index)
    {
        while(parents_[index] != index) { parents_[index] = parents_[parents_[index]]; index = parents_[index]; }
        return index;
    }
    void unite(std::size_t a, std::size_t b)
    {
        a = find(a); b = find(b);
        if(a == b) return;
        if(sizes_[a] < sizes_[b]) std::swap(a, b);
        parents_[b] = a; sizes_[a] += sizes_[b];
    }
    void expand(const GraphCircuit& circuit, const OccurrencePath& path)
    {
        for(const auto& port : circuit.ports)
            terminal(path, port.identity.id, {"circuits", circuit.identity.id, "ports", port.identity.id});
        for(const auto& instance : circuit.instances) {
            auto child = path; child.push_back(instance.identity.id);
            occurrence(child, instance.definition, instance.kind,
                result_.source->source_map.at({"circuits", circuit.identity.id, "instances", instance.identity.id}));
            if(instance.kind == InstanceKind::circuit) expand(*circuits_.at(instance.definition), child);
            else {
                for(const auto& pin : components_.at(instance.definition)->pins)
                    terminal(child, pin.identity.id, {"components", instance.definition, "pins", pin.identity.id});
            }
        }
        for(const auto& net : circuit.nets) {
            const auto span = result_.source->source_map.at({"circuits", circuit.identity.id, "nets", net.identity.id});
            budget(span);
            std::size_t first = 0;
            bool have_first = false;
            for(const auto& endpoint : net.terminals) {
                auto target = path;
                std::string pin;
                if(const auto* local = std::get_if<LocalPortTerminal>(&endpoint)) pin = local->port;
                else { const auto& remote = std::get<InstanceTerminal>(endpoint); target.push_back(remote.instance); pin = remote.terminal; }
                const auto index = nodes_.at({target, pin});
                if(!have_first) { first = index; have_first = true; }
                else unite(first, index);
            }
            // Complete validation guarantees each source net has >=2 terminals.
            const CompiledNet key{path, net.identity.id};
            nets_.emplace(key, first); result_.net_sources.emplace(key, span);
        }
    }
};
}
ConnectivityCompilationResult compile_connectivity(std::string_view original)
{
    auto stage = ConnectivityCompilationStage::declaration;
    try {
        const auto graph = load_project_graph(original);
        if(const auto* error = std::get_if<LockError>(&graph)) return ConnectivityCompilationError{stage, error->code, error->offset};
        stage = ConnectivityCompilationStage::expansion;
        return std::make_shared<const ConnectivityCompilation>(Builder(std::get<0>(graph)).run());
    } catch(const LockError& error) { return ConnectivityCompilationError{stage, error.code, error.offset}; }
    catch(const std::bad_alloc&) { return ConnectivityCompilationError{stage, "memory", 0}; }
}
}
