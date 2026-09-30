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
bool revised(const simnodus::ProjectRevisionResult& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
bool opened(const simnodus::ProjectAcquisitionResult& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
bool saved(const simnodus::ProjectSaveResult& result)
{
    return std::holds_alternative<simnodus::ProjectSaveReceipt>(result);
}
std::optional<simnodus::LiteralResistanceView> view(const simnodus::EditorDocument& document)
{
    return simnodus::literal_resistance(*document.graph(), "main", "left");
}
}
int main(int argc, char** argv)
{
    try {
        simnodus::EditorDocument document;
        const auto empty = document.set_resistance("main", "left", "2");
        const auto* error = std::get_if<simnodus::ProjectRevisionError>(&empty);
        require(error && std::string_view(error->code) == "no-document" && !document.can_undo(), "no-document resistance refusal");
#ifndef _WIN32
        require(!opened(document.open("C:/not-a-native-root", "original.json")), "unsupported physical platform refusal");
        std::cout << "SKIP Windows local NTFS resistance lifecycle\n";
        return 0;
#else
        if(argc != 2) return 2;
        const std::string root = argv[1];
        require(opened(document.open(root, "original.json")), "inert original open");
        const auto original = document.graph();
        const auto selected = view(document);
        require(selected && selected->value == "1" && selected->unit == "kohm" && selected->minimum == "100" && selected->maximum == "10000" && selected->base_unit == "ohm", "view uses selected rc target bounds and fixed unit");
        require(!simnodus::literal_resistance(*original, "rc", "r") && !simnodus::literal_resistance(*original, "main", "absent"), "forwarded and missing views unavailable");
        require(revised(document.set_resistance("main", "left", "1")) && document.graph() == original && !document.can_undo(), "same decoded value creates no step");
        require(revised(document.set_resistance("main", "left", "3.5")) && document.dirty() && document.can_undo() && view(document)->value == "3.5", "literal edit owns validated revision");
        const auto resistance = document.graph();
        require(!revised(document.set_resistance("main", "left", "0.01")) && !revised(document.set_resistance("main", "left", "11")) &&
            !revised(document.set_resistance("main", "left", "1 kohm")) && !revised(document.set_resistance("rc", "r", "2")) &&
            document.graph() == resistance && document.can_undo(), "invalid values and forwarded edit preserve step");
        require(saved(document.save_copy("resistance.json")) && document.graph() == resistance && document.dirty() && document.can_undo() && document.leaf() == "original.json", "resistance copy retains association dirty and history");
        require(revised(document.undo_edit()) && document.graph() == original && !document.dirty() && document.can_redo() && view(document)->value == "1", "undo restores exact original value graph and clean state");
        require(saved(document.save_copy("resistance-undone.json")) && document.can_redo(), "undone copy keeps redo");
        require(!revised(document.set_resistance("main", "left", "")) && revised(document.set_resistance("main", "left", "1")) &&
            !opened(document.open(root, "invalid.json")) && !saved(document.save_copy("occupied.json")) &&
            document.graph() == original && document.can_redo(), "errors no-op open and collision keep redo");
        require(revised(document.redo_edit()) && document.graph() == resistance && view(document)->value == "3.5", "redo restores exact resistance graph");
        require(revised(document.rename_instance("main", "left", "Edited left")), "name after resistance replaces latest transition");
        const auto named = document.graph();
        require(saved(document.save_copy("mixed.json")) && revised(document.undo_edit()) && document.graph() == resistance && view(document)->value == "3.5", "name undo retains prior resistance");
        require(revised(document.redo_edit()) && document.graph() == named && revised(document.undo_edit()) &&
            revised(document.set_resistance("main", "left", "4.7")) && !document.can_redo(), "new resistance after name undo discards stale redo");
        const auto branch = document.graph();
        require(saved(document.save_copy("branch.json")) && revised(document.undo_edit()) && document.graph() == resistance &&
            revised(document.redo_edit()) && document.graph() == branch, "mixed branch has only the new transition");
        require(opened(document.open(root, "branch.json")) && !document.dirty() && !document.can_undo() && !document.can_redo(), "copy Open resets baseline and history");
        require(opened(document.open(root, "escaped.json")), "open owned escaped-value variant");
        const auto escaped = document.graph();
        require(revised(document.set_resistance("main", "left", "3.5")) && revised(document.undo_edit()) &&
            revised(document.set_resistance("main", "left", "1")) && document.graph() == escaped && document.can_redo() && !document.dirty(), "escaped value no-op preserves exact bytes and redo");
        require(revised(document.set_resistance("main", "left", "1.0")) && document.graph() != escaped && document.dirty() && !document.can_redo(), "equal numeric magnitude with different text is an edit");
        require(saved(document.save_copy("escaped-spelling.json")), "persist numeric spelling edit");
        const auto spelled = document.graph();
        require(revised(document.rename("Final resistance project")) && saved(document.save_copy("project-mixed.json")) &&
            revised(document.undo_edit()) && document.graph() == spelled && view(document)->value == "1.0", "project-name undo retains resistance spelling edit");
        simnodus::EditorDocument reopened;
        require(opened(reopened.open(root, "escaped-spelling.json")) && reopened.bytes() == document.bytes() && !reopened.dirty(), "exact resistance copy reopen");
        return 0;
#endif
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
