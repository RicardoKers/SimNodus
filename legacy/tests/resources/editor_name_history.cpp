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
bool error(const simnodus::ProjectRevisionResult& result, const char* code, simnodus::ProjectRevisionStage stage)
{
    const auto* value = std::get_if<simnodus::ProjectRevisionError>(&result);
    return value && std::string_view(value->code) == code && value->stage == stage;
}
bool opened(const simnodus::ProjectAcquisitionResult& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
bool saved(const simnodus::ProjectSaveResult& result)
{
    return std::holds_alternative<simnodus::ProjectSaveReceipt>(result);
}
}
int main(int argc, char** argv)
{
    try {
        using Stage = simnodus::ProjectRevisionStage;
        simnodus::EditorDocument document;
        require(!document.can_undo() && !document.can_redo() &&
            error(document.undo_edit(), "no-document", Stage::base) &&
            error(document.redo_edit(), "no-document", Stage::base), "empty history structured refusals");
#ifndef _WIN32
        require(!opened(document.open("C:/not-a-native-root", "original.json")) &&
            !document.can_undo() && !document.can_redo(), "unsupported open preserves empty history");
        std::cout << "SKIP Windows local NTFS history lifecycle\n";
        return 0;
#else
        if(argc != 2) return 2;
        const std::string root = argv[1];
        require(opened(document.open(root, "original.json")) && !document.can_undo() && !document.can_redo(), "open starts clean history");
        const auto original = document.graph();
        const auto original_name = std::string(document.name());
        require(error(document.undo_edit(), "no-undo", Stage::revision) &&
            error(document.redo_edit(), "no-redo", Stage::revision) && document.graph() == original, "unavailable directions preserve graph");
        require(revised(document.rename(original_name)) && document.graph() == original && !document.can_undo(), "no-op creates no history");
        const std::string project_name = "History \"project\" \\ \xCE\xA9";
        const std::string instance_name = "History \"instance\" \\ \xCE\xA9";
        require(revised(document.rename(project_name)) && document.can_undo() && !document.can_redo() && document.dirty(), "first edit enables undo");
        const auto project = document.graph();
        require(revised(document.undo_edit()) && document.graph() == original && !document.dirty() &&
            !document.can_undo() && document.can_redo(), "first undo restores exact opened graph and clean state");
        require(saved(document.save_copy("clean.json")) && document.can_redo() && !document.dirty() && document.leaf() == "original.json", "clean copy retains redo and association");
        require(error(document.undo_edit(), "no-undo", Stage::revision) && document.graph() == original && document.can_redo(), "second undo refuses without losing redo");
        require(revised(document.rename(original_name)) && !revised(document.rename("")) &&
            !revised(document.rename_instance("absent", "r", "label")) && document.graph() == original && document.can_redo(), "no-op and invalid commands preserve redo");
        require(revised(document.redo_edit()) && document.graph() == project && document.dirty() &&
            document.can_undo() && !document.can_redo(), "redo restores exact edited graph");
        require(error(document.redo_edit(), "no-redo", Stage::revision) && document.graph() == project, "second redo refuses without mutation");
        require(saved(document.save_copy("project.json")) && document.graph() == project && document.can_undo(), "edited copy preserves undo");
        require(revised(document.rename_instance("rc", "r", instance_name)), "instance edit replaces one undo step");
        const auto both = document.graph();
        require(saved(document.save_copy("both.json")), "persist combined names explicitly");
        require(revised(document.undo_edit()) && document.graph() == project && document.dirty() &&
            error(document.undo_edit(), "no-undo", Stage::revision), "later undo keeps earlier edit and has one step");
        require(saved(document.save_copy("undo-latest.json")) && document.can_redo(), "undone copy retains redo");
        require(!opened(document.open(root, "invalid.json")) && !opened(document.open(root, "missing.json")) &&
            !saved(document.save_copy("occupied.json")) && !saved(document.save_copy("original.json")) &&
            document.graph() == project && document.can_redo() && document.dirty() && document.leaf() == "original.json", "failed open and saves retain undone state");
        bool exact_toggles = true;
        for(int i = 0; i < 12; ++i) {
            exact_toggles = exact_toggles && revised(document.redo_edit()) && document.graph() == both;
            exact_toggles = exact_toggles && revised(document.undo_edit()) && document.graph() == project;
        }
        require(exact_toggles && revised(document.redo_edit()) && document.graph() == both, "repeated toggles retain complete immutable graph and source map");
        require(saved(document.save_copy("redo-latest.json")) && document.can_undo(), "redo copy retains undo");
        require(revised(document.undo_edit()) && !revised(document.rename_instance("rc", "r", "")) &&
            revised(document.rename(project_name)) && document.graph() == project && document.can_redo(), "mixed no-op and invalid edit retain redo");
        require(revised(document.rename_instance("rc", "c", "History branch")) && !document.can_redo() && document.can_undo(), "new edit after undo discards former redo");
        const auto branch = document.graph();
        require(saved(document.save_copy("branch.json")) && revised(document.undo_edit()) && document.graph() == project &&
            revised(document.redo_edit()) && document.graph() == branch, "branch restoration targets new transition only");
        require(opened(document.open(root, "branch.json")) && !document.dirty() && !document.can_undo() && !document.can_redo(), "opening saved copy resets baseline and history");
        require(revised(document.rename("another name")) && opened(document.open(root, "branch.json")) &&
            !document.can_undo() && !document.can_redo() && !document.dirty(), "reopening same path clears history");
        require(opened(document.open(root, "escaped.json")), "open owned escaped-name variant");
        const auto escaped = document.graph();
        require(revised(document.rename("temporary")) && revised(document.undo_edit()) && document.graph() == escaped &&
            revised(document.rename(original_name)) && document.graph() == escaped && document.can_redo() && !document.dirty(), "escaped semantic no-op preserves exact graph and redo");
        require(revised(document.redo_edit()) && revised(document.rename(original_name)) && document.dirty(), "decoded manual restoration differs from escaped baseline bytes");
        require(saved(document.save_copy("escaped-restored.json")), "persist manually restored name for byte audit");
        return 0;
#endif
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
