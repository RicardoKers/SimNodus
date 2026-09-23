// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/ideal_rc_compilation.hpp"
#include <algorithm>
#include <new>
#include <set>

namespace simnodus {
namespace {
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
void require(bool condition, IdealRcStage stage, const char* code, Index offset = 0)
{
    if(!condition) throw IdealRcError{stage, code, offset};
}
std::string fixed(std::string value)
{
    if(value.find('.') != std::string::npos) {
        while(value.back() == '0') value.pop_back();
        if(value.back() == '.') value.pop_back();
    }
    return value;
}
class Compiler {
public:
    explicit Compiler(std::shared_ptr<const ConnectivityCompilation> graph)
        : syntax_(*graph->source->declaration->sources.lock.syntax) { result_.connectivity = std::move(graph); }
    IdealRcCompilation run(const std::string& root, const IdealRcRequest& request, bool replay)
    {
        require(request.profile == IdealRcProfile::e01_ideal_rc, IdealRcStage::request, "profile");
        require(!request.reference_port.empty() && request.reference_port.size() <= 64
            && !request.drive_port.empty() && request.drive_port.size() <= 64
            && !request.output_port.empty() && request.output_port.size() <= 64, IdealRcStage::request, "terminal");
        result_.request = request;
        const auto project = object(0), sources = object(project.at("sources"));
        const auto temporal = object(project.at("temporal"));
        if(replay) {
            const auto policy = [&](bool ok, const char* code, const char* key) {
                require(ok, IdealRcStage::profile, code, syntax_.tokens[temporal.at(key)].begin);
            };
            policy(text(temporal.at("mode")) == "known-schedule-replay", "replay-mode", "mode");
            policy(text(temporal.at("debug")) == "disabled", "replay-debug", "debug");
            for(const auto key : {"duration_ns", "exchange_quantum_ns"}) {
                const auto& token = syntax_.tokens[temporal.at(key)];
                policy(std::string_view(syntax_.bytes).substr(token.begin, token.end - token.begin) == "5000000", "replay-time", key);
            }
        }
        require((replay || text(temporal.at("mode")) == "unconfigured")
            && result_.connectivity->source->declaration->targets == 0
            && array(project.at("platforms")).empty() && array(project.at("firmware")).empty(), IdealRcStage::profile, "standalone");
        const auto& graph = *result_.connectivity;
        require(graph.connections.size() == 3, IdealRcStage::profile, "connections");
        const auto root_path = OccurrencePath{graph.source->connectivity.root};
        const auto reference = group({root_path, request.reference_port});
        const auto drive = group({root_path, request.drive_port});
        const auto output = group({root_path, request.output_port});
        require(reference != drive && reference != output && drive != output, IdealRcStage::request, "distinct-nodes");
        result_.nodes.emplace("0", graph.connections[reference]);
        result_.nodes.emplace("in", graph.connections[drive]);
        result_.nodes.emplace("out", graph.connections[output]);
        const auto topology = object(sources.at("topology"));
        std::map<std::string, Fields> components;
        for(const auto index : array(topology.at("components"))) {
            auto fields = object(index);
            components.emplace(text(fields.at("id")), std::move(fields));
        }
        std::set<std::string> descriptors;
        std::size_t count = 0;
        for(const auto& occurrence : graph.occurrences) {
            if(occurrence.kind != InstanceKind::component) continue;
            ++count;
            const auto index = components.at(occurrence.definition).at("model");
            require(syntax_.tokens[index].kind != JsonKind::null, IdealRcStage::profile, "missing-model", syntax_.tokens[index].begin);
            descriptors.insert(text(object(index).at("definition")));
        }
        require(count == 2, IdealRcStage::profile, "components");
        auto physical = verify_local_resources(root, result_.connectivity->source->declaration->sources.lock.requests);
        if(const auto* error = std::get_if<ResourceError>(&physical))
            throw IdealRcError{IdealRcStage::resources, resource_error_name(error->code), error->index, error->system_code, "resource-index"};
        result_.resources = std::get<0>(physical);
        if(replay) {
            const auto schedule = object(temporal.at("schedule"));
            const auto offset = syntax_.tokens[temporal.at("schedule")].begin;
            const auto& inventory = *result_.resources;
            std::size_t index = 0;
            for(; index < inventory.size(); ++index)
                if(inventory[index].dependency == text(schedule.at("dependency"))
                    && inventory[index].resource == text(schedule.at("resource"))) break;
            require(index < inventory.size(), IdealRcStage::profile, "replay-resource", offset);
            const auto& data = inventory[index].data;
            constexpr std::string_view fixed_schedule = "time_ns,drive_uv\n0,3300000\n";
            require(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()) == fixed_schedule,
                IdealRcStage::profile, "replay-schedule", offset);
            result_.replay = FixedRcReplayBinding{5'000'000, 5'000'000, index,
                syntax_.tokens[project.at("temporal")].begin, offset};
        }
        const auto& source_token = syntax_.tokens[project.at("sources")];
        const auto links_bytes = std::string_view(syntax_.bytes).substr(source_token.begin, source_token.end - source_token.begin);
        const auto mappings = object(sources.at("models"));
        std::map<std::string, Fields> assets;
        for(const auto index : array(sources.at("assets"))) {
            auto fields = object(index);assets.emplace(text(fields.at("id")), std::move(fields));
        }
        bool resistor = false, capacitor = false;
        for(const auto& descriptor : descriptors) {
            const auto mapping_index = mappings.at(descriptor);
            require(syntax_.tokens[mapping_index].kind != JsonKind::null, IdealRcStage::binding, "absent", syntax_.tokens[mapping_index].begin);
            const auto mapping = object(mapping_index);
            const auto& asset = assets.at(text(mapping.at("asset")));
            const ResourceSnapshot* captured = nullptr;
            for(const auto& candidate : *result_.resources)
                if(candidate.dependency == text(asset.at("dependency")) && candidate.resource == text(asset.at("resource"))) captured = &candidate;
            require(captured != nullptr, IdealRcStage::binding, "resource");
            const std::string bytes(captured->data.begin(), captured->data.end());
            const auto numeric = bind_passive_numeric(links_bytes, descriptor, bytes);
            if(const auto* error = std::get_if<PassiveNumericError>(&numeric)) {
                const bool source_offset = error->stage == PassiveNumericStage::default_value
                    || (error->stage == PassiveNumericStage::interface && error->interface_stage == PassiveInterfaceStage::source);
                throw IdealRcError{IdealRcStage::binding, error->code,
                    source_offset ? error->offset : source_token.begin + error->offset, 0,
                    source_offset ? "model-source" : "project"};
            }
            const auto binding = std::get<0>(numeric);
            const auto& interface = *binding->interface;
            const auto& model = interface.source->subcircuits[interface.model_index];
            const bool is_resistor = model.kind == spice::PassiveKind::resistor;
            require(binding->occurrences.size() == 1 && !(is_resistor ? resistor : capacitor), IdealRcStage::profile, "elements");
            const auto& row = binding->occurrences.front();
            require(fixed(row.effective.value) == (is_resistor ? "1000" : "0.000001"), IdealRcStage::profile, "value", source_token.begin + row.effective.source_offset);
            require(group({row.path, row.pins[0]}) == (is_resistor ? drive : output)
                && group({row.path, row.pins[1]}) == (is_resistor ? output : reference), IdealRcStage::profile, "wiring");
            result_.elements.push_back({row.path, is_resistor ? 3u : 4u, source_token.begin + row.effective.source_offset,
                graph.occurrence_sources.at(row.path), interface.dependency, interface.resource, {model.begin, model.end}});
            (is_resistor ? resistor : capacitor) = true;
            result_.bindings.push_back(binding);
        }
        require(resistor && capacitor, IdealRcStage::profile, "elements");
        std::sort(result_.elements.begin(), result_.elements.end(), [](const auto& a, const auto& b) { return a.netlist_line < b.netlist_line; });
        // Fixed standalone E-01 profile; recognized single R/C wrappers lower to
        // primitives, so no user source directive/name is copied into the netlist.
        result_.netlist = "SimNodus explicit E-01 ideal RC compilation\nVdrive in 0 3.3\nR1 in out 1000\nC1 out 0 0.000001 IC=0\n"
            ".options reltol=1e-6 abstol=1e-12 vntol=1e-9 method=trap maxord=2\n.save v(in) v(out)\n.tran 1u 5m 0 1u uic\n.end\n";
        return std::move(result_);
    }
private:
    const DeclarationSyntax& syntax_;
    IdealRcCompilation result_;
    Index group(const CompiledTerminal& terminal) const
    {
        const auto& groups = result_.connectivity->connections;
        for(Index i = 0; i < groups.size(); ++i)
            if(std::find(groups[i].terminals.begin(), groups[i].terminals.end(), terminal) != groups[i].terminals.end()) return i;
        throw IdealRcError{IdealRcStage::request, "terminal", 0};
    }
    Fields object(Index index) const
    {
        Fields result;
        for(auto child = index + 1; child < syntax_.tokens[index].next; child = syntax_.tokens[child + 1].next)
            result.emplace(syntax_.tokens[child].decoded, child + 1);
        return result;
    }
    std::vector<Index> array(Index index) const
    {
        std::vector<Index> result;
        for(auto child = index + 1; child < syntax_.tokens[index].next; child = syntax_.tokens[child].next) result.push_back(child);
        return result;
    }
    std::string text(Index index) const { return syntax_.tokens[index].decoded; }
};
IdealRcResult compile(std::string_view bytes, const std::string& root, const IdealRcRequest& request, bool replay)
{
    try {
        const auto graph = compile_connectivity(bytes);
        if(const auto* error = std::get_if<ConnectivityCompilationError>(&graph))
            return IdealRcError{IdealRcStage::declaration, error->code, error->offset};
        return std::make_shared<const IdealRcCompilation>(Compiler(std::get<0>(graph)).run(root, request, replay));
    } catch(const IdealRcError& error) { return error; }
    catch(const std::bad_alloc&) { return IdealRcError{IdealRcStage::profile, "memory", 0}; }
}
}
IdealRcResult compile_ideal_rc(std::string_view bytes, const std::string& root, const IdealRcRequest& request)
{
    return compile(bytes, root, request, false);
}
IdealRcResult compile_fixed_rc_replay(std::string_view bytes, const std::string& root, const IdealRcRequest& request)
{
    return compile(bytes, root, request, true);
}
}
