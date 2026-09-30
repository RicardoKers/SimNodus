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
    ProjectSaveResult save_copy(const std::string& leaf) const;
    const std::shared_ptr<const ProjectGraph>& graph() const { return graph_; }
    const std::string& root() const { return root_; }
    const std::string& leaf() const { return leaf_; }
    std::string_view bytes() const;
    std::string_view name() const;
    bool dirty() const;
private:
    std::shared_ptr<const ProjectGraph> graph_;
    std::shared_ptr<const ProjectGraph> opened_;
    std::string root_, leaf_;
};
}
