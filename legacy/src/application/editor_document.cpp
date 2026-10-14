// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/editor_document.hpp"
#include "application/resource_path_policy_internal.hpp"
#include <new>
#include <utility>

namespace simnodus {
namespace {
const DeclarationSyntax& syntax(const ProjectGraph& graph)
{
    return *graph.declaration->sources.lock.syntax;
}
}
ProjectAcquisitionResult EditorDocument::open(const std::string& root, const std::string& leaf)
{
    // Prepare all owned association strings before committing replacement.
    try {
        auto next_root = root;
        auto next_leaf = leaf;
        auto result = acquire_project(next_root, next_leaf);
        if(const auto* next = std::get_if<std::shared_ptr<const ProjectGraph>>(&result)) {
            graph_ = *next;
            opened_ = *next;
            undo_.reset(); redo_.reset();
            root_.swap(next_root);
            leaf_.swap(next_leaf);
        }
        return result;
    } catch(const std::bad_alloc&) {
        return ProjectAcquisitionError{ProjectAcquisitionStage::physical, "memory"};
    }
}
ProjectRevisionResult EditorDocument::rename(std::string_view name)
{
    if(!graph_) return ProjectRevisionError{ProjectRevisionStage::base, "no-document", 0};
    return accept_revision(rename_project(bytes(), name));
}
ProjectSaveResult EditorDocument::save_copy(const std::string& leaf) const
{
    if(!graph_) return ProjectSaveError{"no-document"};
    try {
        if(resource_policy::lower(leaf) == resource_policy::lower(leaf_))
            return ProjectSaveError{"original-filename"};
        return save_new_project(root_, leaf, bytes());
    } catch(const std::bad_alloc&) { return ProjectSaveError{"memory"}; }
}
ProjectRevisionResult EditorDocument::rename_instance(std::string_view circuit_id,
    std::string_view instance_id, std::string_view name)
{
    if(!graph_) return ProjectRevisionError{ProjectRevisionStage::base, "no-document", 0};
    return accept_revision(simnodus::rename_instance(bytes(), circuit_id, instance_id, name));
}
ProjectRevisionResult EditorDocument::accept_revision(ProjectRevisionResult result)
{
    if(auto* next = std::get_if<std::shared_ptr<const ProjectGraph>>(&result)) {
        if(syntax(**next).bytes == bytes()) { *next = graph_; return result; }
        undo_ = graph_;
        redo_.reset();
        graph_ = *next;
    }
    return result;
}
ProjectRevisionResult EditorDocument::set_resistance(std::string_view circuit_id,
    std::string_view instance_id, std::string_view value)
{
    if(!graph_) return ProjectRevisionError{ProjectRevisionStage::base, "no-document", 0};
    return accept_revision(revise_resistance(bytes(), circuit_id, instance_id, value));
}
ProjectRevisionResult EditorDocument::set_capacitance(std::string_view circuit_id,
    std::string_view instance_id, std::string_view value)
{
    if(!graph_) return ProjectRevisionError{ProjectRevisionStage::base, "no-document", 0};
    return accept_revision(revise_capacitance(bytes(), circuit_id, instance_id, value));
}
ProjectRevisionResult EditorDocument::undo_edit()
{
    if(!graph_) return ProjectRevisionError{ProjectRevisionStage::base, "no-document", 0};
    if(!undo_) return ProjectRevisionError{ProjectRevisionStage::revision, "no-undo", 0};
    redo_ = graph_;
    graph_ = std::move(undo_);
    return graph_;
}
ProjectRevisionResult EditorDocument::redo_edit()
{
    if(!graph_) return ProjectRevisionError{ProjectRevisionStage::base, "no-document", 0};
    if(!redo_) return ProjectRevisionError{ProjectRevisionStage::revision, "no-redo", 0};
    undo_ = graph_;
    graph_ = std::move(redo_);
    return graph_;
}
std::string_view EditorDocument::bytes() const
{
    return graph_ ? std::string_view(syntax(*graph_).bytes) : std::string_view{};
}
std::string_view EditorDocument::name() const
{
    if(!graph_) return {};
    const auto& tokens = syntax(*graph_).tokens;
    for(std::size_t key = 1; key < tokens[0].next; key = tokens[key + 1].next)
        if(tokens[key].decoded == "name") return tokens[key + 1].decoded;
    return {};
}
bool EditorDocument::dirty() const
{
    return graph_ && syntax(*graph_).bytes != syntax(*opened_).bytes;
}
}
