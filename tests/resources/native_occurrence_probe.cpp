// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/symbol_preview.hpp"
#include "application/editor_document.hpp"
#include "../schema/graph_probe_output.hpp"
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
void require(bool value, const char* message)
{
    if(!value) throw std::runtime_error(message);
    std::cout << "PASS " << message << '\n';
}
void write(const simnodus::ComponentOccurrenceView& value)
{
    std::cout << "{\"path\":";
    graph_probe::array(value.path, [](const auto& id) { graph_probe::quote(id); });
    std::cout << ",\"source_circuit\":"; graph_probe::quote(value.source_circuit);
    std::cout << ",\"source_instance\":"; graph_probe::quote(value.source_instance);
    std::cout << ",\"component\":"; graph_probe::quote(value.component);
    std::cout << ",\"name\":"; graph_probe::quote(value.name);
    std::cout << ",\"parameters\":{";
    bool first = true;
    for(const auto& [id, parameter] : value.parameters) {
        if(!first) std::cout << ',';
        first = false;
        graph_probe::quote(id); std::cout << ":{\"value\":"; graph_probe::quote(parameter.value);
        std::cout << ",\"unit\":"; graph_probe::quote(parameter.unit);
        std::cout << ",\"origin\":"; graph_probe::quote(parameter.origin);
        std::cout << '}';
    }
    std::cout << "}}\n";
}
bool graph_result(const simnodus::ProjectRevisionResult& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
int lifecycle(const std::string& root, const std::string& artwork_root)
{
#ifndef _WIN32
    (void)root; (void)artwork_root;
    std::cout << "SKIP Windows local NTFS occurrence artwork lifecycle\n";
    return 0;
#else
    simnodus::EditorDocument document;
    require(std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(document.open(root, "original.json")) &&
        !document.dirty() && !document.can_undo(), "inert clean Open with no resources in document directory");
    auto original = document.graph();
    auto bytes = std::string(document.bytes());
    const std::vector<std::string> path{"main", "right", "r"};
    const auto selection = simnodus::inspect_component_occurrence(*original, path);
    require(selection && selection->source_circuit == "rc" && selection->source_instance == "r" &&
        selection->component == "resistor" && selection->parameters.at("resistance").value == "2200", "owned full path/source definition and applied value");
    const auto result = simnodus::capture_fixture_occurrence(*original, path, artwork_root);
    require(std::holds_alternative<std::shared_ptr<const simnodus::OccurrenceArtworkCapture>>(result), "explicit independent occurrence capture");
    const auto capture = std::get<0>(result);
    require(root != artwork_root && capture->symbol->requested_root == artwork_root && capture->symbol->resources->size() == 1 &&
        capture->symbol->resources->front().data.size() == 227, "one separately rooted owned SVG only");
    require(document.graph() == original && document.bytes() == bytes && !document.dirty() && !document.can_undo() &&
        !original->declaration->sources.lock.resources_verified && !original->declaration->sources.lock.containment_verified &&
        !original->declaration->sources.resource_interfaces_verified && !original->declaration->runtime_profile_verified,
        "capture grants no edit/history/global resource or execution authority");
    require(graph_result(document.set_resistance("main", "right", "3.5")), "existing narrow right resistance edit");
    auto edited = document.graph();
    const auto current = simnodus::inspect_component_occurrence(*edited, path);
    require(current && current->path == capture->occurrence.path && current->parameters.at("resistance").value == "3500" &&
        capture->occurrence.parameters.at("resistance").value == "2200" &&
        simnodus::fixture_capture_matches(*edited, current->component, *capture->symbol), "current values requery while captured identity/artwork remain owned");
    require(graph_result(document.undo_edit()) && simnodus::inspect_component_occurrence(*document.graph(), path)->parameters.at("resistance").value == "2200" &&
        graph_result(document.redo_edit()) && document.graph() == edited && simnodus::fixture_capture_matches(*edited, "resistor", *capture->symbol),
        "one-step resistance history preserves full occurrence ID and binding");
    require(graph_result(document.rename_instance("rc", "r", "Occurrence resistor")) &&
        simnodus::inspect_component_occurrence(*document.graph(), path)->name == "Occurrence resistor" &&
        capture->occurrence.name == "r" && simnodus::fixture_capture_matches(*document.graph(), "resistor", *capture->symbol),
        "name revision refreshes labels without changing artwork identity");
    require(graph_result(document.undo_edit()) && document.graph() == edited &&
        std::holds_alternative<simnodus::ProjectSaveReceipt>(document.save_copy("occurrence-copy.json")), "explicit create-only copy after restoring label");
    require(document.dirty() && document.leaf() == "original.json" && document.can_redo(), "copy retains original association/dirty/history");
    require(std::holds_alternative<simnodus::ProjectAcquisitionError>(document.open(root, "missing.json")) && document.graph() == edited &&
        std::holds_alternative<simnodus::SymbolPreviewError>(simnodus::capture_fixture_occurrence(*edited, path, artwork_root + "/missing")),
        "failed Open/capture retains independent current graph and original capture");
    require(std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(document.open(root, "occurrence-copy.json")) &&
        !document.dirty() && !document.can_undo() && simnodus::inspect_component_occurrence(*document.graph(), path)->parameters.at("resistance").value == "3500",
        "explicit reopen resolves same full ID with persisted right value");
    document = simnodus::EditorDocument{}; original.reset(); edited.reset();
    std::fill(bytes.begin(), bytes.end(), 'x');
    const auto& data = capture->symbol->resources->front().data;
    require(selection->path == path && selection->name == "r" && capture->occurrence.path == path && capture->symbol->pins &&
        simnodus::symbols::recognize_fixture_artwork(std::string_view(reinterpret_cast<const char*>(data.data()), data.size())).has_value(),
        "view/identity/bytes/pins survive all document/graph/raw owner release");
    return 0;
#endif
}
}
int main(int argc, char** argv)
{
    try {
        if(argc == 4 && std::string_view(argv[1]) == "--lifecycle") return lifecycle(argv[2], argv[3]);
        if(argc < 2 || std::string_view(argv[1]) != "--select") return 2;
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        std::string raw(simnodus::declaration_max_bytes + 1, '\0');
        std::cin.read(raw.data(), static_cast<std::streamsize>(raw.size()));
        raw.resize(static_cast<std::size_t>(std::cin.gcount()));
        auto loaded = simnodus::load_project_graph(raw);
        if(const auto* error = std::get_if<simnodus::LockError>(&loaded)) {
            std::cout << "{\"error\":"; graph_probe::quote(error->code); std::cout << "}\n";
            return 1;
        }
        auto graph = std::get<0>(loaded);
        std::vector<std::string> path;
        for(int index = 2; index < argc; ++index) path.emplace_back(argv[index]);
        const auto selected = simnodus::inspect_component_occurrence(*graph, path);
        if(!selected) { std::cout << "{\"error\":\"occurrence\"}\n"; return 1; }
        graph.reset(); loaded = simnodus::LockError{"test-owner-released", 0};
        std::fill(raw.begin(), raw.end(), 'x');
        write(*selected);
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
