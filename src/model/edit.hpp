// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Undoable document changes (UI-08). An Edit records, for every element it
// touches, the element before and after the change; applying and reverting
// are therefore exact for any operation. The application wraps each Edit in
// a QUndoCommand; the model itself has no undo stack.
#pragma once

#include "schematic.hpp"

#include <optional>
#include <string>
#include <vector>

namespace simnodus::model {

struct Change {
    Id id = no_id;
    std::optional<Element> before; // empty: the element did not exist
    std::optional<Element> after;  // empty: the element is deleted
};

class Edit {
public:
    Edit() = default;
    explicit Edit(std::string description) : description_(std::move(description)) {}

    const std::string& description() const { return description_; }
    const std::vector<Change>& changes() const { return changes_; }
    bool empty() const { return changes_.empty() && !settings_ && !blocks_; }

    void apply(Schematic& schematic) const;
    void revert(Schematic& schematic) const;

    // Settings changes are recorded alongside element changes.
    void set_settings(const SimulationSettings& before, const SimulationSettings& after);
    // So are changes to the project's blocks.
    void set_blocks(const std::map<std::string, Block>& before, const std::map<std::string, Block>& after);

private:
    friend Edit make_edit(const Schematic&, const Schematic&, std::string);
    std::string description_;
    std::vector<Change> changes_;
    std::optional<std::pair<SimulationSettings, SimulationSettings>> settings_;
    std::optional<std::pair<std::map<std::string, Block>, std::map<std::string, Block>>> blocks_;
};

// Difference between two states of a document: every element that was added,
// removed or changed, and the settings. Operations work on a copy of the
// schematic and turn the result into an Edit with this function.
Edit make_edit(const Schematic& before, const Schematic& after, std::string description);

} // namespace simnodus::model
