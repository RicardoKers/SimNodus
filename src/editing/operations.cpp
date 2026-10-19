// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "operations.hpp"

#include "io/project_file.hpp"

#include <algorithm>
#include <climits>
#include <map>

namespace simnodus::editing {
namespace {

using namespace simnodus::model;

// Pin positions of a component (empty if its type is not in the library).
std::vector<Point> pin_points(const library::ComponentLibrary& library, const Component& c)
{
    std::vector<Point> result;
    if (const auto* d = library.find(c.type)) {
        for (const auto& pin : d->pins) {
            result.push_back(library::pin_position(*d, pin, c));
        }
    }
    return result;
}

// Moves wire end `index` (0 or last) to `target`, keeping the polyline
// orthogonal by sliding the neighbouring vertex or adding a corner.
void move_end(std::vector<Point>& points, std::size_t index, Point target)
{
    const bool first = index == 0;
    const std::size_t neighbour = first ? 1 : points.size() - 2;
    const Point old_end = points[index];
    const Point next = points[neighbour];
    const bool horizontal = old_end.y == next.y && old_end.x != next.x;
    if (points.size() == 2) {
        if (target.x == next.x || target.y == next.y) {
            points[index] = target;
            return;
        }
        // Keep the direction at the far end; turn once near the moved end.
        const Point corner = horizontal ? Point{target.x, next.y} : Point{next.x, target.y};
        if (first) {
            points = {target, corner, next};
        } else {
            points = {next, corner, target};
        }
        return;
    }
    if (horizontal) {
        points[neighbour].y = target.y;
    } else {
        points[neighbour].x = target.x;
    }
    points[index] = target;
}

// Re-attaches unselected wires whose ends were on `moved` points.
void follow(Schematic& work, const Schematic& original, const std::set<Id>& selected,
    const std::map<Point, Point>& moved)
{
    for (const auto& wire : original.wires()) {
        if (selected.count(wire.id) || wire.points.size() < 2) {
            continue;
        }
        const auto a = moved.find(wire.points.front());
        const auto b = moved.find(wire.points.back());
        if (a == moved.end() && b == moved.end()) {
            continue;
        }
        Wire updated = wire;
        if (a != moved.end() && b != moved.end() && a->second - a->first == b->second - b->first) {
            const Point delta = a->second - a->first;
            for (auto& p : updated.points) {
                p = p + delta;
            }
        } else {
            if (a != moved.end()) {
                move_end(updated.points, 0, a->second);
            }
            if (b != moved.end()) {
                move_end(updated.points, updated.points.size() - 1, b->second);
            }
            updated.points = normalize_polyline(updated.points);
        }
        if (updated.points.size() >= 2) {
            work.put(updated);
        } else {
            work.erase(wire.id); // collapsed to a point
        }
    }
}

// Points that define the extent of a selection.
std::vector<Point> selection_points(const Schematic& s, const library::ComponentLibrary& library,
    const std::set<Id>& ids)
{
    std::vector<Point> result;
    for (Id id : ids) {
        if (const auto* c = s.component(id)) {
            result.push_back(c->position);
            for (const auto& p : pin_points(library, *c)) {
                result.push_back(p);
            }
        } else if (const auto* w = s.wire(id)) {
            result.insert(result.end(), w->points.begin(), w->points.end());
        } else if (const auto* l = s.label(id)) {
            result.push_back(l->position);
        } else if (const auto* n = s.note(id)) {
            result.push_back(n->position);
        } else if (const auto* b = s.bus(id)) {
            result.insert(result.end(), b->points.begin(), b->points.end());
        } else if (const auto* sp = s.splitter(id)) {
            result.push_back(sp->position);
            for (const auto& p : splitter_pins(*sp)) {
                result.push_back(p);
            }
        }
    }
    return result;
}

Point center_of(const std::vector<Point>& points)
{
    int min_x = INT_MAX, min_y = INT_MAX, max_x = INT_MIN, max_y = INT_MIN;
    for (const auto& p : points) {
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    // Integer center keeps everything on the grid.
    return {min_x + (max_x - min_x) / 2, min_y + (max_y - min_y) / 2};
}

// Applies a point transformation to the selection and re-attaches wires.
template <typename PointMap, typename OrientationMap>
Edit transform_items(const Schematic& schematic, const library::ComponentLibrary& library, const std::set<Id>& ids,
    PointMap map_point, OrientationMap map_orientation, const std::string& description)
{
    Schematic work = schematic;
    std::map<Point, Point> moved;
    for (Id id : ids) {
        if (const auto* c = schematic.component(id)) {
            Component updated = *c;
            updated.position = map_point(c->position);
            updated.orientation = map_orientation(c->orientation);
            const auto before = pin_points(library, *c);
            const auto after = pin_points(library, updated);
            for (std::size_t i = 0; i < before.size() && i < after.size(); ++i) {
                moved.emplace(before[i], after[i]);
            }
            work.put(updated);
        } else if (const auto* w = schematic.wire(id)) {
            Wire updated = *w;
            for (auto& p : updated.points) {
                p = map_point(p);
            }
            moved.emplace(w->points.front(), updated.points.front());
            moved.emplace(w->points.back(), updated.points.back());
            work.put(updated);
        } else if (const auto* l = schematic.label(id)) {
            NetLabel updated = *l;
            updated.position = map_point(l->position);
            work.put(updated);
        } else if (const auto* n = schematic.note(id)) {
            TextNote updated = *n;
            updated.position = map_point(n->position);
            work.put(updated);
        } else if (const auto* b = schematic.bus(id)) {
            Bus updated = *b;
            for (auto& p : updated.points) {
                p = map_point(p);
            }
            work.put(updated);
        } else if (const auto* sp = schematic.splitter(id)) {
            Splitter updated = *sp;
            updated.position = map_point(sp->position);
            updated.orientation = map_orientation(sp->orientation);
            // Wires on its bit pins stay attached, as on a component's pins.
            const auto before = splitter_pins(*sp);
            const auto after = splitter_pins(updated);
            for (std::size_t i = 0; i < before.size(); ++i) {
                moved.emplace(before[i], after[i]);
            }
            work.put(updated);
        }
    }
    follow(work, schematic, ids, moved);
    return make_edit(schematic, work, description);
}

} // namespace

std::vector<Point> normalize_polyline(std::vector<Point> points)
{
    points.erase(std::unique(points.begin(), points.end()), points.end());
    bool changed = true;
    while (changed && points.size() >= 3) {
        changed = false;
        for (std::size_t i = 1; i + 1 < points.size(); ++i) {
            const Point& a = points[i - 1];
            const Point& b = points[i];
            const Point& c = points[i + 1];
            if ((a.x == b.x && b.x == c.x) || (a.y == b.y && b.y == c.y)) {
                points.erase(points.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }
    return points;
}

bool is_orthogonal(const std::vector<Point>& points)
{
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (points[i].x != points[i - 1].x && points[i].y != points[i - 1].y) {
            return false;
        }
    }
    return true;
}

Edit add_component(Schematic& schematic, const library::ComponentDefinition& definition, Point position,
    Orientation orientation, Id* created)
{
    Schematic work = schematic;
    Component c;
    c.id = schematic.allocate_id();
    c.type = definition.type;
    c.designator = next_designator(schematic, definition.prefix);
    c.position = position;
    c.orientation = {normalized_rotation(orientation.rotation), orientation.mirrored};
    c.parameters = definition.default_parameters();
    work.put(c);
    if (created) {
        *created = c.id;
    }
    return make_edit(schematic, work, "Place " + c.designator);
}

Edit add_wire(Schematic& schematic, std::vector<Point> points, Id* created)
{
    points = normalize_polyline(std::move(points));
    if (points.size() < 2 || !is_orthogonal(points)) {
        return {};
    }
    Schematic work = schematic;
    Wire w{schematic.allocate_id(), points};
    work.put(w);
    if (created) {
        *created = w.id;
    }
    return make_edit(schematic, work, "Draw wire");
}

Edit add_label(Schematic& schematic, Point position, const std::string& name, Id* created)
{
    if (name.empty()) {
        return {};
    }
    Schematic work = schematic;
    NetLabel l{schematic.allocate_id(), position, name};
    work.put(l);
    if (created) {
        *created = l.id;
    }
    return make_edit(schematic, work, "Place net label " + name);
}

Edit add_note(Schematic& schematic, Point position, const std::string& text, Id* created)
{
    if (text.find_first_not_of(" \t\r\n") == std::string::npos) {
        return {};
    }
    Schematic work = schematic;
    TextNote n{schematic.allocate_id(), position, text};
    work.put(n);
    if (created) {
        *created = n.id;
    }
    return make_edit(schematic, work, "Place text note");
}

Edit set_note_text(const Schematic& schematic, Id note, const std::string& text)
{
    const auto* n = schematic.note(note);
    if (!n || n->text == text) {
        return {};
    }
    Schematic work = schematic;
    if (text.find_first_not_of(" \t\r\n") == std::string::npos) {
        work.erase(note);
        return make_edit(schematic, work, "Delete text note");
    }
    TextNote updated = *n;
    updated.text = text;
    work.put(updated);
    return make_edit(schematic, work, "Edit text note");
}

Edit add_bus(Schematic& schematic, std::vector<Point> points, const std::string& name, Id* created)
{
    points = normalize_polyline(std::move(points));
    if (points.size() < 2 || !is_orthogonal(points) || name.empty()) {
        return {};
    }
    Schematic work = schematic;
    Bus b{schematic.allocate_id(), points, name};
    work.put(b);
    if (created) {
        *created = b.id;
    }
    return make_edit(schematic, work, "Draw bus " + name);
}

Edit add_splitter(Schematic& schematic, Point position, const std::string& bus, int first, int count, Id* created)
{
    if (bus.empty() || first < 0 || count < 1 || count > 64) {
        return {};
    }
    Schematic work = schematic;
    Splitter s{schematic.allocate_id(), position, {}, bus, first, count};
    work.put(s);
    if (created) {
        *created = s.id;
    }
    return make_edit(schematic, work, "Place splitter " + bus);
}

Edit rename_bus(const Schematic& schematic, Id bus, const std::string& name)
{
    const auto* b = schematic.bus(bus);
    if (!b || name.empty() || b->name == name) {
        return {};
    }
    Schematic work = schematic;
    for (const auto& s : schematic.splitters()) {
        bool on_bus = false;
        for (std::size_t i = 0; i + 1 < b->points.size() && !on_bus; ++i) {
            on_bus = model::on_segment(s.position, b->points[i], b->points[i + 1]);
        }
        if (on_bus && s.bus == b->name) {
            Splitter updated = s;
            updated.bus = name;
            work.put(updated);
        }
    }
    Bus renamed = *b;
    renamed.name = name;
    work.put(renamed);
    return make_edit(schematic, work, "Rename bus " + b->name + " to " + name);
}

Edit set_splitter(const Schematic& schematic, Id splitter, const std::string& bus, int first, int count)
{
    const auto* s = schematic.splitter(splitter);
    if (!s || bus.empty() || first < 0 || count < 1 || count > 64) {
        return {};
    }
    Schematic work = schematic;
    Splitter updated = *s;
    updated.bus = bus;
    updated.first = first;
    updated.count = count;
    if (updated == *s) {
        return {};
    }
    work.put(updated);
    return make_edit(schematic, work, "Change splitter " + bus);
}

Edit add_probe(Schematic& schematic, const Probe& probe, Id* created)
{
    Schematic work = schematic;
    Probe p = probe;
    p.id = schematic.allocate_id();
    work.put(p);
    if (created) {
        *created = p.id;
    }
    return make_edit(schematic, work, "Add probe");
}

Edit delete_items(const Schematic& schematic, const std::set<Id>& ids)
{
    Schematic work = schematic;
    for (Id id : ids) {
        work.erase(id);
    }
    return make_edit(schematic, work, ids.size() == 1 ? "Delete item" : "Delete items");
}

Edit move_items(const Schematic& schematic, const library::ComponentLibrary& library, const std::set<Id>& ids,
    Point delta)
{
    if (delta == Point{}) {
        return {};
    }
    return transform_items(
        schematic, library, ids, [delta](Point p) { return p + delta; }, [](Orientation o) { return o; }, "Move");
}

Edit rotate_items(const Schematic& schematic, const library::ComponentLibrary& library, const std::set<Id>& ids)
{
    if (ids.empty()) {
        return {};
    }
    Point center;
    if (ids.size() == 1 && schematic.component(*ids.begin())) {
        center = schematic.component(*ids.begin())->position;
    } else {
        const auto points = selection_points(schematic, library, ids);
        if (points.empty()) {
            return {};
        }
        center = center_of(points);
    }
    return transform_items(
        schematic, library, ids, [center](Point p) { return rotate_cw(p, center); },
        [](Orientation o) { return Orientation{normalized_rotation(o.rotation + 90), o.mirrored}; }, "Rotate");
}

Edit mirror_items(const Schematic& schematic, const library::ComponentLibrary& library, const std::set<Id>& ids)
{
    if (ids.empty()) {
        return {};
    }
    int axis = 0;
    if (ids.size() == 1 && schematic.component(*ids.begin())) {
        axis = schematic.component(*ids.begin())->position.x;
    } else {
        const auto points = selection_points(schematic, library, ids);
        if (points.empty()) {
            return {};
        }
        axis = center_of(points).x;
    }
    // Mirror after rotation r equals rotation -r after mirror (see geometry.hpp).
    return transform_items(
        schematic, library, ids, [axis](Point p) { return mirror_x(p, axis); },
        [](Orientation o) { return Orientation{normalized_rotation(360 - o.rotation), !o.mirrored}; }, "Mirror");
}

Edit set_parameter(const Schematic& schematic, Id component, const std::string& name, const std::string& value)
{
    const auto* c = schematic.component(component);
    if (!c) {
        return {};
    }
    Schematic work = schematic;
    Component updated = *c;
    updated.parameters[name] = value;
    work.put(updated);
    return make_edit(schematic, work, "Set " + c->designator + " " + name);
}

Edit set_model(const Schematic& schematic, Id component, const std::string& parameter, const std::string& name,
    const std::string& statement)
{
    const auto* c = schematic.component(component);
    if (!c || name.empty()) {
        return {};
    }
    Schematic work = schematic;
    Component updated = *c;
    updated.parameters[parameter] = name;
    if (statement.empty()) {
        updated.models.erase(parameter);
    } else {
        updated.models[parameter] = statement;
    }
    updated.models.erase(parameter + ".pins"); // a pin assignment belongs to one model
    work.put(updated);
    return make_edit(schematic, work, "Set " + c->designator + " model " + name);
}

Edit set_model_pins(const Schematic& schematic, Id component, const std::string& parameter,
    const std::vector<std::string>& pins)
{
    const auto* c = schematic.component(component);
    if (!c) {
        return {};
    }
    Schematic work = schematic;
    Component updated = *c;
    std::string list;
    for (const auto& pin : pins) {
        list += (list.empty() ? "" : ",") + pin;
    }
    if (list.empty()) {
        updated.models.erase(parameter + ".pins");
    } else {
        updated.models[parameter + ".pins"] = list;
    }
    if (updated == *c) {
        return {};
    }
    work.put(updated);
    return make_edit(schematic, work, "Set " + c->designator + " model pins");
}

Edit set_designator(const Schematic& schematic, Id component, const std::string& designator)
{
    const auto* c = schematic.component(component);
    if (!c || designator.empty()) {
        return {};
    }
    Schematic work = schematic;
    Component updated = *c;
    updated.designator = designator;
    work.put(updated);
    return make_edit(schematic, work, "Rename " + c->designator + " to " + designator);
}

Edit move_text(const Schematic& schematic, Id component, bool value, Point offset)
{
    const auto* c = schematic.component(component);
    if (!c) {
        return {};
    }
    Schematic work = schematic;
    Component updated = *c;
    (value ? updated.value_offset : updated.designator_offset) = offset;
    work.put(updated);
    return make_edit(schematic, work, "Move text");
}

Edit set_settings(const Schematic& schematic, const SimulationSettings& settings)
{
    Schematic work = schematic;
    work.set_settings(settings);
    return make_edit(schematic, work, "Change simulation settings");
}

Edit set_blocks(const Schematic& schematic, const std::vector<Block>& blocks, std::string description)
{
    Schematic work = schematic;
    auto all = work.blocks();
    for (const auto& b : blocks) {
        all[b.name] = b;
    }
    work.set_blocks(std::move(all));
    return make_edit(schematic, work, std::move(description));
}

std::string copy_items(const Schematic& schematic, const std::set<Id>& ids)
{
    std::vector<Element> elements;
    std::map<std::string, Block> blocks; // the blocks the copied parts use travel with them
    for (Id id : ids) {
        if (auto e = schematic.get(id)) {
            elements.push_back(*e);
            if (const auto* c = std::get_if<Component>(&*e)) {
                if (const auto* b = schematic.block(model::block_name(c->type))) {
                    blocks.emplace(b->name, *b);
                }
            }
        }
    }
    return io::elements_to_json_text(elements, blocks);
}

Edit paste_items(Schematic& schematic, const library::ComponentLibrary& library, const std::string& clipboard,
    Point offset, std::set<Id>* created)
{
    std::string error;
    std::map<std::string, Block> blocks;
    const auto elements = io::elements_from_json_text(clipboard, error, &blocks);
    if (!elements || elements->empty()) {
        return {};
    }
    Schematic work = schematic;
    // Blocks the project does not have yet come along; a block of the same name stays as it is.
    auto merged = work.blocks();
    merged.insert(blocks.begin(), blocks.end());
    work.set_blocks(std::move(merged));
    std::map<Id, Id> renamed;
    for (const auto& e : *elements) {
        renamed[element_id(e)] = schematic.allocate_id();
    }
    for (const auto& e : *elements) {
        std::visit(
            [&](auto item) {
                using T = std::decay_t<decltype(item)>;
                item.id = renamed[item.id];
                if constexpr (std::is_same_v<T, Component>) {
                    item.position = item.position + offset;
                    const auto* d = library.find(item.type);
                    if (work.component_by_designator(item.designator)) {
                        std::string prefix = d ? d->prefix : std::string();
                        if (prefix.empty()) {
                            prefix = item.designator.substr(0, item.designator.find_first_of("0123456789"));
                        }
                        item.designator = next_designator(work, prefix);
                    }
                } else if constexpr (std::is_same_v<T, Wire>) {
                    for (auto& p : item.points) {
                        p = p + offset;
                    }
                } else if constexpr (std::is_same_v<T, NetLabel> || std::is_same_v<T, TextNote>
                    || std::is_same_v<T, Splitter>) {
                    item.position = item.position + offset;
                } else if constexpr (std::is_same_v<T, Bus>) {
                    for (auto& p : item.points) {
                        p = p + offset;
                    }
                } else {
                    // Probes follow their pasted targets; others keep pointing at the original.
                    auto remap = [&](Anchor& a) {
                        if (auto it = renamed.find(a.item); it != renamed.end()) {
                            a.item = it->second;
                            a.point = a.point + offset;
                        }
                    };
                    remap(item.target);
                    if (item.reference) {
                        remap(*item.reference);
                    }
                }
                work.put(item);
                if (created) {
                    created->insert(item.id);
                }
            },
            e);
    }
    return make_edit(schematic, work, "Paste");
}

} // namespace simnodus::editing
