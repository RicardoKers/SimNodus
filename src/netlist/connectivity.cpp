// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "connectivity.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <numeric>
#include <set>

namespace simnodus::netlist {
namespace {

using model::Schematic;

class UnionFind {
public:
    int add()
    {
        parent_.push_back(static_cast<int>(parent_.size()));
        return parent_.back();
    }
    int find(int x)
    {
        while (parent_[static_cast<std::size_t>(x)] != x) {
            auto& p = parent_[static_cast<std::size_t>(x)];
            p = parent_[static_cast<std::size_t>(p)];
            x = p;
        }
        return x;
    }
    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b) {
            parent_[static_cast<std::size_t>(std::max(a, b))] = std::min(a, b);
        }
    }

private:
    std::vector<int> parent_;
};

bool on_wire(Point p, const model::Wire& w)
{
    for (std::size_t i = 1; i < w.points.size(); ++i) {
        if (model::on_segment(p, w.points[i - 1], w.points[i])) {
            return true;
        }
    }
    return false;
}

bool is_ground_name(const std::string& name)
{
    std::string upper;
    for (char c : name) {
        upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return upper == "0" || upper == "GND";
}

struct PinAt {
    PinRef pin;
    Point at;
};

std::vector<PinAt> all_pins(const Schematic& s, const library::ComponentLibrary& library)
{
    std::vector<PinAt> pins;
    for (const auto& c : s.components()) {
        if (const auto* d = library.find(c.type)) {
            for (const auto& pin : d->pins) {
                pins.push_back({{c.id, pin.name}, library::pin_position(*d, pin, c)});
            }
        }
    }
    return pins;
}

} // namespace

std::string sanitize_name(const std::string& text)
{
    std::string result;
    for (char c : text) {
        result += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    }
    return result.empty() ? std::string("_") : result;
}

Connectivity build_connectivity(const Schematic& s, const library::ComponentLibrary& library)
{
    Connectivity result;
    UnionFind uf;
    const auto pins = all_pins(s, library);
    std::vector<int> pin_node, wire_node, label_node;
    for (std::size_t i = 0; i < pins.size(); ++i) {
        pin_node.push_back(uf.add());
    }
    for (std::size_t i = 0; i < s.wires().size(); ++i) {
        wire_node.push_back(uf.add());
    }
    for (std::size_t i = 0; i < s.labels().size(); ++i) {
        label_node.push_back(uf.add());
    }
    // Splitters (UI-19): bit k of the left side is bit k of the right side. A
    // wire pin connects at its point like a component pin; the bits of a pin on
    // a bus are nets named after that bus ("D0", "D1", ...).
    struct Tap {
        Point at;
        int node = 0;
    };
    std::vector<Tap> taps;
    struct Bit {
        int node = -1;    // a wire pin's node
        std::string name; // or the named net of a bus bit
    };
    std::vector<std::pair<std::vector<Bit>, std::vector<Bit>>> splitter_bits;
    auto bus_at = [&s](const Point& p) {
        for (const auto& b : s.buses()) {
            for (std::size_t i = 0; i + 1 < b.points.size(); ++i) {
                if (model::on_segment(p, b.points[i], b.points[i + 1])) {
                    return b.name;
                }
            }
        }
        return std::string();
    };
    for (const auto& sp : s.splitters()) {
        std::vector<Bit> left, right;
        for (const auto& pin : model::splitter_layout(sp)) {
            auto& side = pin.left ? left : right;
            std::string bus = bus_at(pin.at);
            if (pin.width == 1 && bus.empty()) {
                taps.push_back({pin.at, uf.add()});
                side.push_back({taps.back().node, {}});
                continue;
            }
            if (bus.empty() && pin.left) {
                bus = sp.bus; // files before 0.4 named the bus in the splitter
            }
            for (int i = 0; i < pin.width; ++i) {
                if (bus.empty()) {
                    side.push_back({uf.add(), {}}); // a bus pin on no bus connects nothing
                } else {
                    side.push_back({-1, bus + std::to_string((pin.left ? sp.first : 0) + i)});
                }
            }
        }
        splitter_bits.emplace_back(std::move(left), std::move(right));
    }
    std::map<std::string, int> named; // net name -> node (labels, tied nets)
    auto named_node = [&](const std::string& name) {
        const std::string key = is_ground_name(name) ? std::string("0") : name;
        auto it = named.find(key);
        if (it == named.end()) {
            it = named.emplace(key, uf.add()).first;
        }
        return it->second;
    };

    // Pins: with other pins, wires and labels at the same point; tied nets.
    for (std::size_t i = 0; i < pins.size(); ++i) {
        for (std::size_t j = i + 1; j < pins.size(); ++j) {
            if (pins[i].at == pins[j].at) {
                uf.unite(pin_node[i], pin_node[j]);
            }
        }
        for (std::size_t w = 0; w < s.wires().size(); ++w) {
            if (on_wire(pins[i].at, s.wires()[w])) {
                uf.unite(pin_node[i], wire_node[w]);
            }
        }
        const auto* c = s.component(pins[i].pin.first);
        const auto* d = c ? library.find(c->type) : nullptr;
        if (d && !d->ties_net.empty()) {
            uf.unite(pin_node[i], named_node(d->ties_net));
        }
        if (d && !d->ties_net_parameter.empty()) {
            const auto it = c->parameters.find(d->ties_net_parameter);
            const auto* p = d->parameter(d->ties_net_parameter);
            const std::string name = it != c->parameters.end() ? it->second : p ? p->default_value : std::string();
            if (!name.empty()) {
                uf.unite(pin_node[i], named_node(name));
            }
        }
    }
    // Wires: a vertex or end of one on the other.
    for (std::size_t a = 0; a < s.wires().size(); ++a) {
        for (std::size_t b = 0; b < s.wires().size(); ++b) {
            if (a == b) {
                continue;
            }
            for (const auto& p : s.wires()[a].points) {
                if (on_wire(p, s.wires()[b])) {
                    uf.unite(wire_node[a], wire_node[b]);
                    break;
                }
            }
        }
    }
    // Labels: at a pin or on a wire, and by name.
    for (std::size_t l = 0; l < s.labels().size(); ++l) {
        const auto& label = s.labels()[l];
        uf.unite(label_node[l], named_node(label.name));
        for (std::size_t w = 0; w < s.wires().size(); ++w) {
            if (on_wire(label.position, s.wires()[w])) {
                uf.unite(label_node[l], wire_node[w]);
            }
        }
        for (std::size_t i = 0; i < pins.size(); ++i) {
            if (pins[i].at == label.position) {
                uf.unite(label_node[l], pin_node[i]);
            }
        }
    }

    for (const auto& [left, right] : splitter_bits) {
        auto node = [&](const Bit& bit) { return bit.node >= 0 ? bit.node : named_node(bit.name); };
        for (std::size_t k = 0; k < std::min(left.size(), right.size()); ++k) {
            uf.unite(node(left[k]), node(right[k]));
        }
    }
    for (const auto& tap : taps) {
        for (std::size_t w = 0; w < s.wires().size(); ++w) {
            if (on_wire(tap.at, s.wires()[w])) {
                uf.unite(tap.node, wire_node[w]);
            }
        }
        for (std::size_t l = 0; l < s.labels().size(); ++l) {
            if (s.labels()[l].position == tap.at) {
                uf.unite(tap.node, label_node[l]);
            }
        }
        for (std::size_t i = 0; i < pins.size(); ++i) {
            if (pins[i].at == tap.at) {
                uf.unite(tap.node, pin_node[i]);
            }
        }
    }

    // Build nets in a deterministic order: by first pin, then wire, then label.
    std::map<int, int> root_net;
    auto net_for = [&](int node) {
        const int root = uf.find(node);
        auto it = root_net.find(root);
        if (it == root_net.end()) {
            it = root_net.emplace(root, static_cast<int>(result.nets.size())).first;
            result.nets.emplace_back();
        }
        return it->second;
    };
    for (std::size_t i = 0; i < pins.size(); ++i) {
        const int n = net_for(pin_node[i]);
        result.nets[static_cast<std::size_t>(n)].pins.push_back(pins[i].pin);
        result.pin_net[pins[i].pin] = n;
    }
    for (std::size_t w = 0; w < s.wires().size(); ++w) {
        const int n = net_for(wire_node[w]);
        result.nets[static_cast<std::size_t>(n)].wires.push_back(s.wires()[w].id);
        result.wire_net[s.wires()[w].id] = n;
    }
    for (std::size_t l = 0; l < s.labels().size(); ++l) {
        const int n = net_for(label_node[l]);
        auto& net = result.nets[static_cast<std::size_t>(n)];
        net.labels.push_back(s.labels()[l].id);
        net.label_names.push_back(s.labels()[l].name);
        result.label_net[s.labels()[l].id] = n;
    }
    for (const auto& tap : taps) {
        const int n = net_for(tap.node);
        result.nets[static_cast<std::size_t>(n)].taps += 1;
        result.tap_net[tap.at] = n;
    }
    if (const auto ground = named.find("0"); ground != named.end()) {
        if (const auto it = root_net.find(uf.find(ground->second)); it != root_net.end()) {
            result.nets[static_cast<std::size_t>(it->second)].ground = true;
        }
    }
    // Tied nets other than ground (rare) behave like labels.
    for (const auto& [name, node] : named) {
        if (name != "0") {
            if (const auto it = root_net.find(uf.find(node)); it != root_net.end()) {
                auto& names = result.nets[static_cast<std::size_t>(it->second)].label_names;
                if (std::find(names.begin(), names.end(), name) == names.end()) {
                    names.push_back(name);
                }
            }
        }
    }

    // Names: ground, else the first label alphabetically, else N001...
    // ngspice ignores case, so uniqueness is checked case-insensitively.
    auto folded = [](std::string text) {
        for (auto& ch : text) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return text;
    };
    std::set<std::string> used{"0"};
    int counter = 0;
    for (auto& net : result.nets) {
        if (net.ground) {
            net.name = "0";
            continue;
        }
        std::vector<std::string> names;
        for (const auto& n : net.label_names) {
            if (n.empty() || n.front() != hidden_label_mark) {
                names.push_back(n);
            }
        }
        std::sort(names.begin(), names.end());
        std::string chosen;
        for (const auto& n : names) {
            const std::string candidate = sanitize_name(n);
            if (!used.count(folded(candidate))) {
                chosen = candidate;
                break;
            }
        }
        while (chosen.empty()) {
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "N%03d", ++counter);
            if (!used.count(folded(buffer))) {
                chosen = buffer;
            }
        }
        used.insert(folded(chosen));
        net.name = chosen;
    }

    // Junction dots and open ends: count connections at candidate points.
    std::map<Point, int> degree;
    std::set<Point> candidates;
    for (const auto& p : pins) {
        degree[p.at] += 1;
        candidates.insert(p.at);
    }
    for (const auto& w : s.wires()) {
        if (w.points.size() >= 2) {
            candidates.insert(w.points.front());
            candidates.insert(w.points.back());
        }
    }
    for (const auto& point : candidates) {
        for (const auto& w : s.wires()) {
            if (w.points.size() < 2) {
                continue;
            }
            if (point == w.points.front() || point == w.points.back()) {
                degree[point] += 1;
            } else if (on_wire(point, w)) {
                degree[point] += 2; // passes through: two directions
            }
        }
    }
    for (const auto& [point, count] : degree) {
        if (count >= 3) {
            result.junctions.push_back(point);
        }
    }
    std::set<Point> label_points;
    for (const auto& l : s.labels()) {
        label_points.insert(l.position);
    }
    for (const auto& tap : taps) {
        label_points.insert(tap.at); // a wire ending on a bit pin is connected
    }
    for (const auto& w : s.wires()) {
        if (w.points.size() < 2) {
            continue;
        }
        for (const auto& end : {w.points.front(), w.points.back()}) {
            if (degree[end] <= 1 && !label_points.count(end)) {
                result.dangling_ends.push_back(end);
            }
        }
    }
    for (const auto& p : pins) {
        const auto& net = result.nets[static_cast<std::size_t>(result.pin_net[p.pin])];
        const auto* c = s.component(p.pin.first);
        const auto* d = c ? library.find(c->type) : nullptr;
        const auto* pin_definition = d ? d->pin(p.pin.second) : nullptr;
        const bool tied = d && (!d->ties_net.empty() || !d->ties_net_parameter.empty()
                                   || (pin_definition && pin_definition->optional));
        if (!tied && net.pins.size() == 1 && net.wires.empty() && net.labels.empty() && net.taps == 0) {
            result.unconnected_pins.push_back(p.pin);
        }
    }
    return result;
}

std::optional<int> Connectivity::net_at(const Schematic& s, const library::ComponentLibrary& library, Point point) const
{
    for (const auto& p : all_pins(s, library)) {
        if (p.at == point) {
            return pin_net.at(p.pin);
        }
    }
    for (const auto& w : s.wires()) {
        if (on_wire(point, w)) {
            return wire_net.at(w.id);
        }
    }
    for (const auto& l : s.labels()) {
        if (l.position == point) {
            return label_net.at(l.id);
        }
    }
    if (const auto tap = tap_net.find(point); tap != tap_net.end()) {
        return tap->second;
    }
    return std::nullopt;
}

const Net* Connectivity::net_of_pin(const PinRef& pin) const
{
    const auto it = pin_net.find(pin);
    return it == pin_net.end() ? nullptr : &nets[static_cast<std::size_t>(it->second)];
}

} // namespace simnodus::netlist
