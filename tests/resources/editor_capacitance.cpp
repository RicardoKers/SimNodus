// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/editor_document.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* check)
{
    if(!value) throw std::runtime_error(check);
    std::cout << "PASS " << check << '\n';
}
bool revised(const simnodus::ProjectRevisionResult& r) { return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(r); }
bool opened(const simnodus::ProjectAcquisitionResult& r) { return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(r); }
bool saved(const simnodus::ProjectSaveResult& r) { return std::holds_alternative<simnodus::ProjectSaveReceipt>(r); }
std::optional<simnodus::LiteralValueView> view(const simnodus::EditorDocument& d) { return simnodus::literal_capacitance(*d.graph(), "main", "right"); }
}
int main(int argc, char** argv)
{
    try {
        simnodus::EditorDocument d;
        const auto empty = d.set_capacitance("main", "right", "470");
        const auto* error = std::get_if<simnodus::ProjectRevisionError>(&empty);
        require(error && std::string_view(error->code) == "no-document" && !d.can_undo(), "no-document capacitance refusal");
#ifndef _WIN32
        require(!opened(d.open("C:/not-a-native-root", "original.json")), "unsupported physical platform refusal");
        std::cout << "SKIP Windows local NTFS capacitance lifecycle\n";
        return 0;
#else
        if(argc != 2) return 2;
        const std::string root = argv[1];
        require(opened(d.open(root, "original.json")), "inert original open");
        const auto original = d.graph();
        require(view(d) && view(d)->value == "220" && view(d)->unit == "nF" && view(d)->minimum == "0.000000001" && view(d)->maximum == "0.001" && view(d)->base_unit == "F", "fixed-unit target capacitance view");
        require(!simnodus::literal_capacitance(*original, "main", "left") && !simnodus::literal_capacitance(*original, "rc", "c"), "missing and forwarded capacitance unavailable");
        require(revised(d.set_capacitance("main", "right", "220")) && d.graph() == original && !d.can_undo(), "same value no-op is clean");
        require(revised(d.set_capacitance("main", "right", "470")) && d.dirty() && d.can_undo(), "owned capacitance revision enables one step");
        const auto cap = d.graph();
        require(!revised(d.set_capacitance("main", "right", "0.1")) && !revised(d.set_capacitance("main", "right", "1000001")) &&
            !revised(d.set_capacitance("main", "right", "NaN")) && !revised(d.set_capacitance("main", "left", "470")) && d.graph() == cap, "invalid range quantity and missing literal retain graph");
        require(saved(d.save_copy("capacitance.json")) && d.leaf() == "original.json" && d.dirty() && d.can_undo(), "explicit copy retains association and history");
        require(revised(d.undo_edit()) && d.graph() == original && !d.dirty() && saved(d.save_copy("capacitance-undone.json")) && d.can_redo(), "exact undo clean copy retains redo");
        require(revised(d.set_capacitance("main", "right", "220")) && !revised(d.set_capacitance("main", "right", "")) &&
            !opened(d.open(root, "invalid.json")) && !saved(d.save_copy("occupied.json")) && d.graph() == original && d.can_redo(), "no-op and failures retain redo");
        require(revised(d.redo_edit()) && d.graph() == cap && revised(d.set_resistance("main", "right", "3.5")), "resistance after capacitance shares history");
        const auto rc = d.graph();
        require(saved(d.save_copy("rc.json")) && revised(d.undo_edit()) && d.graph() == cap && view(d)->value == "470", "resistance undo retains capacitance");
        require(revised(d.redo_edit()) && d.graph() == rc && revised(d.set_capacitance("main", "right", "330")), "capacitance after resistance replaces latest step");
        const auto branch = d.graph();
        require(saved(d.save_copy("branch.json")) && revised(d.undo_edit()) && d.graph() == rc && revised(d.redo_edit()) && d.graph() == branch &&
            revised(d.undo_edit()) && !revised(d.undo_edit()), "mixed R C history retains exactly one transition");
        require(revised(d.rename("Capacitance project")) && saved(d.save_copy("project.json")) && revised(d.undo_edit()) && d.graph() == rc && view(d)->value == "470", "project name undo retains both quantities");
        require(revised(d.rename_instance("main", "right", "Edited right")) && saved(d.save_copy("all.json")) &&
            revised(d.undo_edit()) && d.graph() == rc && view(d)->value == "470", "instance name undo retains both quantities");
        require(opened(d.open(root, "all.json")) && !d.dirty() && !d.can_undo() && !d.can_redo(), "copy reopen resets history and baseline");
        require(opened(d.open(root, "escaped.json")), "open escaped capacitance variant");
        const auto escaped = d.graph();
        require(revised(d.set_capacitance("main", "right", "470")) && revised(d.undo_edit()) && revised(d.set_capacitance("main", "right", "220")) &&
            d.graph() == escaped && !d.dirty() && d.can_redo(), "escaped decoded no-op retains exact graph and redo");
        require(revised(d.set_capacitance("main", "right", "220.0")) && d.dirty() && !d.can_redo() && saved(d.save_copy("escaped-spelling.json")), "equal magnitude new spelling is a byte edit");
        simnodus::EditorDocument reopened;
        require(opened(reopened.open(root, "escaped-spelling.json")) && reopened.bytes() == d.bytes() && !reopened.dirty(), "exact capacitance copy reopen");
        return 0;
#endif
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
