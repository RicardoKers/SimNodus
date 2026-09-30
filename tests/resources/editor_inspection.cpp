// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/editor_document.hpp"
#include "application/project_inspection.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* check)
{
    if(!value) throw std::runtime_error(check);
    std::cout << "PASS " << check << '\n';
}
template<class Result> bool revised(const Result& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
bool saved(const simnodus::ProjectSaveResult& result)
{
    return std::holds_alternative<simnodus::ProjectSaveReceipt>(result);
}
bool value(const simnodus::ProjectGraph& graph, const char* circuit, const char* instance,
    const std::vector<std::string>& path, const char* parameter, const char* expected)
{
    for(const auto* row : simnodus::inspect_instance_parameters(graph, circuit, instance))
        if(row->path == path) {
            const auto entry = row->parameters.find(parameter);
            return entry != row->parameters.end() && entry->second.value == expected;
        }
    return false;
}
}
int main(int argc, char** argv)
{
    try {
        if(argc != 2) return 2;
        const std::string root = argv[1];
        std::ifstream input(std::filesystem::path(root) / "original.json", std::ios::binary);
        std::string raw((std::istreambuf_iterator<char>(input)), {});
        auto loaded = simnodus::load_project_graph(raw);
        require(revised(loaded), "owned inspection fixture load");
        const auto retained = std::get<0>(loaded);
        const auto views = simnodus::inspect_instance_parameters(*retained, "rc", "r");
        raw.clear();
        require(views.size() == 2 && views[0]->path == std::vector<std::string>{"main", "left", "r"} &&
            views[1]->path == std::vector<std::string>{"main", "right", "r"}, "retained reused occurrence paths independent of caller input");
        require(views[0]->parameters.at("resistance").origin == "containing-circuit:resistance" &&
            views[0]->parameters.at("resistance").value == "1000" && views[1]->parameters.at("resistance").value == "2200",
            "immediate forwarding origins and distinct values");
#ifndef _WIN32
        std::cout << "SKIP Windows local NTFS edit copy lifecycle\n";
        return 0;
#else
        simnodus::EditorDocument document;
        require(revised(document.open(root, "original.json")), "inert native document open");
        const auto original = document.graph();
        const auto bytes = std::string(document.bytes());
        const auto left = simnodus::inspect_instance_parameters(*original, "main", "left");
        require(left.size() == 1 && left[0]->parameters.at("capacitance").origin == "default" &&
            left[0]->parameters.at("capacitance").value == "0.000001", "default capacitance is inspectable without becoming editable");
        const auto initial_rows = simnodus::inspect_instance_parameters(*original, "rc", "c");
        require(initial_rows.size() == 2 && document.graph() == original && document.bytes() == bytes &&
            !document.dirty() && !document.can_undo() && !document.can_redo(), "inspection creates no edit history bytes or dirty state");
        require(revised(document.set_capacitance("main", "right", "470")) &&
            value(*document.graph(), "rc", "c", {"main", "right", "c"}, "capacitance", "0.000000470"), "current graph refreshes propagated capacitance");
        const auto cap = document.graph();
        require(initial_rows[1]->parameters.at("capacitance").value == "0.000000220" && original->declaration->sources.lock.syntax->bytes == bytes,
            "retained old views remain immutable after edit");
        require(saved(document.save_copy("cap.json")) && document.graph() == cap && document.leaf() == "original.json" &&
            document.dirty() && document.can_undo(), "explicit inspection copy retains source and history");
        require(revised(document.undo_edit()) && document.graph() == original && !document.dirty() &&
            value(*document.graph(), "rc", "c", {"main", "right", "c"}, "capacitance", "0.000000220"), "undo resolves exact original values");
        require(saved(document.save_copy("undone.json")) && document.can_redo(), "undone copy retains redo");
        require(revised(document.set_capacitance("main", "right", "220")) && !revised(document.set_capacitance("main", "right", "NaN")) &&
            !revised(document.open(root, "invalid.json")) && !saved(document.save_copy("occupied.json")) && document.graph() == original &&
            document.can_redo(), "no-op invalid edit open and occupied copy preserve inspection graph and redo");
        require(revised(document.redo_edit()) && document.graph() == cap &&
            value(*document.graph(), "rc", "c", {"main", "right", "c"}, "capacitance", "0.000000470"), "redo resolves current capacitance");
        require(revised(document.set_resistance("main", "right", "3.5")) &&
            value(*document.graph(), "rc", "r", {"main", "right", "r"}, "resistance", "3500") &&
            value(*document.graph(), "rc", "r", {"main", "left", "r"}, "resistance", "1000"), "upstream resistance refresh affects only matching occurrence");
        const auto rc = document.graph();
        require(saved(document.save_copy("rc.json")) && revised(document.undo_edit()) && document.graph() == cap &&
            revised(document.redo_edit()) && document.graph() == rc, "mixed quantity restore and explicit persisted copy");
        require(revised(document.rename("Longer inspection project name")) && revised(document.rename_instance("main", "right", "Longer inspection right name")) &&
            value(*document.graph(), "main", "right", {"main", "right"}, "resistance", "3500") &&
            value(*document.graph(), "rc", "c", {"main", "right", "c"}, "capacitance", "0.000000470"), "name edits shift provenance without changing effective values");
        require(revised(document.open(root, "rc.json")) && !document.dirty() && !document.can_undo() && !document.can_redo() &&
            value(*document.graph(), "rc", "r", {"main", "right", "r"}, "resistance", "3500"), "explicit copy reopen starts fresh inspectable baseline");
        return 0;
#endif
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
