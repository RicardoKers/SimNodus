// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_acquisition.hpp"
#include "application/project_revision.hpp"
#include "application/project_save.hpp"

namespace simnodus {
// One inert external document. Save Copy never changes its source association
// or grants resource/execution authority. Presentation supplies explicit input.
class EditorDocument {
public:
    ProjectAcquisitionResult open(const std::string& root, const std::string& leaf);
    ProjectRevisionResult rename(std::string_view name);
    ProjectRevisionResult rename_instance(std::string_view circuit_id,
        std::string_view instance_id, std::string_view name);
    ProjectRevisionResult set_resistance(std::string_view circuit_id,
        std::string_view instance_id, std::string_view value);
    ProjectSaveResult save_copy(const std::string& leaf) const;
    // One applied name/resistance transition, independent of text drafts.
    ProjectRevisionResult undo_edit();
    ProjectRevisionResult redo_edit();
    bool can_undo() const { return bool(undo_); }
    bool can_redo() const { return bool(redo_); }
    const std::shared_ptr<const ProjectGraph>& graph() const { return graph_; }
    const std::string& root() const { return root_; }
    const std::string& leaf() const { return leaf_; }
    std::string_view bytes() const;
    std::string_view name() const;
    bool dirty() const;
private:
    ProjectRevisionResult accept_revision(ProjectRevisionResult result);
    std::shared_ptr<const ProjectGraph> graph_;
    std::shared_ptr<const ProjectGraph> opened_;
    // At most one counterpart is retained. Immutable snapshots are already
    // validated; restoration changes neither I/O authority nor association.
    std::shared_ptr<const ProjectGraph> undo_, redo_;
    std::string root_, leaf_;
};
}
