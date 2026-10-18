// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Document operations of the Circuit Editor (UI-05..UI-11). Each function
// returns an Edit without changing the schematic; the caller applies it
// (through the undo stack). IDs for new elements are reserved from the
// schematic, which is why it is passed by non-const reference.
#pragma once

#include "library/component_library.hpp"
#include "model/edit.hpp"
#include "model/schematic.hpp"

#include <set>
#include <string>
#include <vector>

namespace simnodus::editing {

using model::Edit;
using model::Id;
using model::Point;

// New component with library defaults and the next free designator.
Edit add_component(model::Schematic& schematic, const library::ComponentDefinition& definition, Point position,
    model::Orientation orientation, Id* created = nullptr);

// Orthogonal wire through `points` (consecutive duplicates and collinear
// vertices are removed). Returns an empty Edit for a degenerate wire.
Edit add_wire(model::Schematic& schematic, std::vector<Point> points, Id* created = nullptr);

Edit add_label(model::Schematic& schematic, Point position, const std::string& name, Id* created = nullptr);

Edit add_probe(model::Schematic& schematic, const model::Probe& probe, Id* created = nullptr);

// Text note (UI-17); an empty or blank text gives an empty Edit.
Edit add_note(model::Schematic& schematic, Point position, const std::string& text, Id* created = nullptr);
// New text of a note; a blank text deletes the note.
Edit set_note_text(const model::Schematic& schematic, Id note, const std::string& text);

// Deletes the elements. Probes pointing at deleted items stay and become
// invalid (they are reported, never silently rebound).
Edit delete_items(const model::Schematic& schematic, const std::set<Id>& ids);

// Moves the elements by `delta`. Unselected wires that end on a pin of a
// moved component, or on a moved wire end, stay attached and orthogonal.
Edit move_items(const model::Schematic& schematic, const library::ComponentLibrary& library, const std::set<Id>& ids,
    Point delta);

// Rotates the elements 90 degrees clockwise (a single component about its
// origin, several about the center of their bounding box); attached wires follow.
Edit rotate_items(const model::Schematic& schematic, const library::ComponentLibrary& library, const std::set<Id>& ids);

// Mirrors the elements left-right; attached wires follow.
Edit mirror_items(const model::Schematic& schematic, const library::ComponentLibrary& library, const std::set<Id>& ids);

Edit set_parameter(const model::Schematic& schematic, Id component, const std::string& name, const std::string& value);
Edit set_designator(const model::Schematic& schematic, Id component, const std::string& designator);
// Moves a component's designator or value text (UI-11); `value` selects which.
Edit move_text(const model::Schematic& schematic, Id component, bool value, Point offset);
Edit set_settings(const model::Schematic& schematic, const model::SimulationSettings& settings);

// Clipboard: the selected elements as JSON text, and pasting it back with
// new IDs, designators renumbered where taken, shifted by `offset`.
std::string copy_items(const model::Schematic& schematic, const std::set<Id>& ids);
Edit paste_items(model::Schematic& schematic, const library::ComponentLibrary& library, const std::string& clipboard,
    Point offset, std::set<Id>* created = nullptr);

// Removes consecutive duplicate points and collinear interior vertices.
std::vector<Point> normalize_polyline(std::vector<Point> points);
bool is_orthogonal(const std::vector<Point>& points);

} // namespace simnodus::editing
