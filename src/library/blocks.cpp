// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "blocks.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace simnodus::library {
namespace {

bool ground_name(std::string name)
{
    for (auto& ch : name) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return name == "0" || name == "GND";
}

// Whether a circuit, or a block inside it, holds a microcontroller board.
bool has_board(const model::Schematic& circuit, const ComponentLibrary& library)
{
    for (const auto& c : circuit.components()) {
        if (const auto* d = library.find(c.type); d && d->mcu) {
            return true;
        }
    }
    return std::any_of(circuit.blocks().begin(), circuit.blocks().end(),
        [&](const auto& entry) { return has_board(*entry.second.schematic, library); });
}

// Approximate width of bold symbol text, in grid units.
double text_width(const std::string& text, double size)
{
    return 0.62 * size * static_cast<double>(text.size());
}

Graphic text_graphic(const std::string& text, double x, double y, double size)
{
    Graphic g;
    g.kind = Graphic::Kind::Text;
    g.points = {{x, y}};
    g.text = text;
    g.text_size = size;
    return g;
}

Graphic line(double x1, double y1, double x2, double y2)
{
    Graphic g;
    g.kind = Graphic::Kind::Line;
    g.points = {{x1, y1}, {x2, y2}};
    return g;
}

int even_ceiling(double value)
{
    const int n = static_cast<int>(std::ceil(value - 1e-9));
    return n % 2 == 0 ? n : n + 1;
}

} // namespace

std::vector<BlockPort> block_ports(const model::Schematic& circuit, const ComponentLibrary& library,
    std::vector<std::string>& problems)
{
    std::vector<BlockPort> ports;
    std::set<std::string> names;
    for (const auto& c : circuit.components()) {
        if (c.type != port_type) {
            continue;
        }
        const auto* d = library.find(port_type);
        auto value = [&](const char* parameter) {
            const auto it = c.parameters.find(parameter);
            if (it != c.parameters.end()) {
                return it->second;
            }
            const auto* p = d ? d->parameter(parameter) : nullptr;
            return p ? p->default_value : std::string();
        };
        const std::string name = value("name");
        if (name.empty()) {
            problems.push_back("a port has no name");
            continue;
        }
        if (ground_name(name)) {
            problems.push_back("a port is named " + name + "; ground is shared with the circuit around the block "
                                                           "anyway, so use a ground symbol inside the block instead");
            continue;
        }
        if (!names.insert(name).second) {
            continue; // ports with the same name are one pin
        }
        std::string side = value("side");
        if (side != "right" && side != "top" && side != "bottom") {
            side = "left";
        }
        ports.push_back({name, side, c.id, c.position});
    }
    if (ports.empty() && problems.empty()) {
        problems.push_back("it has no ports; place Block port parts (Blocks in the parts tree) on the signals that "
                           "leave the block");
    }
    if (has_board(circuit, library)) {
        problems.push_back("a block cannot hold a microcontroller board");
    }
    if (!problems.empty()) {
        return {};
    }
    std::stable_sort(ports.begin(), ports.end(), [](const BlockPort& a, const BlockPort& b) {
        const bool vertical = a.side == "left" || a.side == "right";
        if (vertical) {
            return a.at.y != b.at.y ? a.at.y < b.at.y : a.at.x < b.at.x;
        }
        return a.at.x != b.at.x ? a.at.x < b.at.x : a.at.y < b.at.y;
    });
    return ports;
}

std::optional<ComponentDefinition> block_definition(const model::Block& block, const ComponentLibrary& library,
    std::vector<std::string>& problems)
{
    const ComponentLibrary inner = with_blocks(library, *block.schematic);
    const auto ports = block_ports(*block.schematic, inner, problems);
    if (ports.empty()) {
        return std::nullopt;
    }
    std::map<std::string, std::vector<const BlockPort*>> sides;
    for (const auto& p : ports) {
        sides[p.side].push_back(&p);
    }
    auto count = [&](const char* side) { return static_cast<int>(sides[side].size()); };
    auto widest = [&](const char* side) {
        double w = 0;
        for (const auto* p : sides[side]) {
            w = std::max(w, text_width(p->name, 0.8));
        }
        return w;
    };
    constexpr double label_size = 0.8;
    constexpr double name_size = 1.0;
    const int rows = std::max({count("left"), count("right"), 1});
    const int columns = std::max(count("top"), count("bottom"));
    const bool top = count("top") > 0;
    const bool bottom = count("bottom") > 0;
    // Height: two grid units per row, and a row for the top and bottom labels.
    const int height = 2 * (rows + 1) + (top ? 2 : 0) + (bottom ? 2 : 0);
    const double inside = widest("left") + widest("right") + text_width(block.name, name_size) + 2.0;
    const int width = std::max({4, 2 * (columns + 1), even_ceiling(inside)});
    const int left = -width / 2;
    const int right = width / 2;
    const int upper = -height / 2;
    const int lower = height / 2;
    const int middle = (top ? 1 : 0) - (bottom ? 1 : 0); // centre of the rows between the label rows

    ComponentDefinition d;
    d.type = model::block_type(block.name);
    d.name = block.name;
    d.category = "Blocks";
    d.prefix = "U";
    d.description = "Block drawn by the user" + (block.source.empty() ? std::string() : ": " + block.source);
    d.block = block.schematic;
    Graphic frame;
    frame.kind = Graphic::Kind::Rect;
    frame.points = {{static_cast<double>(left), static_cast<double>(upper)},
        {static_cast<double>(width), static_cast<double>(height)}};
    d.graphics.push_back(frame);
    d.graphics.push_back(text_graphic(block.name, (widest("left") - widest("right")) / 2.0, middle, name_size));
    for (const auto& [side, list] : sides) {
        const int n = static_cast<int>(list.size());
        for (int i = 0; i < n; ++i) {
            const auto& name = list[static_cast<std::size_t>(i)]->name;
            const int along = -(n - 1) + 2 * i; // centred, two grid units apart
            const double label = text_width(name, label_size) / 2.0;
            Pin pin;
            pin.name = name;
            if (side == "left") {
                pin.at = {left - 1, middle + along};
                pin.direction = PinDirection::Left;
                d.graphics.push_back(line(left - 1, pin.at.y, left, pin.at.y));
                d.graphics.push_back(text_graphic(name, left + 0.4 + label, pin.at.y, label_size));
            } else if (side == "right") {
                pin.at = {right + 1, middle + along};
                pin.direction = PinDirection::Right;
                d.graphics.push_back(line(right, pin.at.y, right + 1, pin.at.y));
                d.graphics.push_back(text_graphic(name, right - 0.4 - label, pin.at.y, label_size));
            } else if (side == "top") {
                pin.at = {along, upper - 1};
                pin.direction = PinDirection::Up;
                d.graphics.push_back(line(pin.at.x, upper - 1, pin.at.x, upper));
                d.graphics.push_back(text_graphic(name, pin.at.x, upper + 0.9, label_size));
            } else {
                pin.at = {along, lower + 1};
                pin.direction = PinDirection::Down;
                d.graphics.push_back(line(pin.at.x, lower, pin.at.x, lower + 1));
                d.graphics.push_back(text_graphic(name, pin.at.x, lower - 0.9, label_size));
            }
            d.pins.push_back(pin);
        }
    }
    d.designator_offset = {left, upper - (top ? 2 : 1)};
    d.show_designator = true;
    return d;
}

ComponentDefinition subcircuit_definition(const SpiceSubcircuit& subcircuit)
{
    ComponentDefinition d;
    d.type = subcircuit_prefix + subcircuit.name;
    d.name = subcircuit.name;
    d.category = "Subcircuits";
    d.prefix = "U";
    d.description = subcircuit.description.empty() ? "Vendor subcircuit from the model folders" : subcircuit.description;
    const int count = static_cast<int>(subcircuit.ports.size());
    const int left = (count + 1) / 2;
    const int rows = std::max(left, 1);
    const int height = 2 * rows + 2;
    const int top = -(height / 2);
    auto text = [](std::string value, double x, double y, double size) {
        Graphic g;
        g.kind = Graphic::Kind::Text;
        g.text = std::move(value);
        g.points = {{x, y}};
        g.text_size = size;
        return g;
    };
    Graphic body;
    body.kind = Graphic::Kind::Rect;
    body.points = {{-4, static_cast<double>(top)}, {8, static_cast<double>(height)}};
    d.graphics.push_back(body);
    d.graphics.push_back(text(subcircuit.name, 0, top - 0.8, 0.8));
    for (int i = 0; i < count; ++i) {
        // DIP order: down the left side, then up the right side.
        const bool on_left = i < left;
        const int row = on_left ? i : count - 1 - i;
        const double y = top + 2 + 2 * row;
        const double edge = on_left ? -4 : 4;
        const double tip = on_left ? -6 : 6;
        Graphic lead;
        lead.kind = Graphic::Kind::Line;
        lead.points = {{tip, y}, {edge, y}};
        d.graphics.push_back(lead);
        d.graphics.push_back(text(subcircuit.ports[static_cast<std::size_t>(i)], edge + (on_left ? 1.4 : -1.4), y, 0.6));
        Pin pin;
        pin.name = subcircuit.ports[static_cast<std::size_t>(i)];
        pin.at = {static_cast<int>(tip), static_cast<int>(y)};
        pin.direction = on_left ? PinDirection::Left : PinDirection::Right;
        d.pins.push_back(pin);
    }
    d.designator_offset = {5, top - 1};
    d.value_offset = {5, -top + 1};
    Parameter model;
    model.name = "model";
    model.label = "Subcircuit";
    model.kind = Parameter::Kind::Model;
    model.default_value = subcircuit.name;
    model.description = "The vendor subcircuit this part runs; Pins... says which port is which pin";
    model.subcircuit = subcircuit.ports;
    d.parameters.push_back(model);
    // The kept subcircuit replaces this template (a comment) in the netlist.
    d.spice = {"* {ref}: vendor subcircuit"};
    d.initial_models["model"] = subcircuit.text;
    return d;
}

ComponentLibrary with_blocks(const ComponentLibrary& base, const model::Schematic& schematic,
    std::vector<std::string>* problems)
{
    ComponentLibrary result = base;
    // Vendor subcircuit parts: one definition per type, from the text its first component keeps.
    for (const auto& c : schematic.components()) {
        if (c.type.rfind(subcircuit_prefix, 0) != 0 || result.find(c.type)) {
            continue;
        }
        const auto kept = c.models.find("model");
        std::string why = "it keeps no subcircuit";
        std::optional<SpiceSubcircuit> sub;
        if (kept != c.models.end()) {
            sub = parse_subcircuit_text(kept->second, &why);
        }
        if (sub) {
            auto d = subcircuit_definition(*sub);
            d.type = c.type; // the name it was placed with, even if another model was chosen later
            result.add_definition(std::move(d));
        } else if (problems) {
            problems->push_back(c.designator + " (" + c.type.substr(std::string(subcircuit_prefix).size())
                + ") cannot be used: " + why + ".");
        }
    }
    for (const auto& [name, block] : schematic.blocks()) {
        std::vector<std::string> why;
        if (auto d = block_definition(block, base, why)) {
            result.add_definition(std::move(*d));
        } else if (problems) {
            for (const auto& text : why) {
                problems->push_back("The block " + name + " cannot be used: " + text + ".");
            }
        }
    }
    return result;
}

} // namespace simnodus::library
