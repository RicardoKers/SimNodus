// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "schematic.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace simnodus::model {
namespace {

template <typename T>
const T* find(const std::vector<T>& items, Id id)
{
    const auto it = std::lower_bound(items.begin(), items.end(), id, [](const T& e, Id v) { return e.id < v; });
    return it != items.end() && it->id == id ? &*it : nullptr;
}

template <typename T>
void put_sorted(std::vector<T>& items, const T& element)
{
    const auto it =
        std::lower_bound(items.begin(), items.end(), element.id, [](const T& e, Id v) { return e.id < v; });
    if (it != items.end() && it->id == element.id) {
        *it = element;
    } else {
        items.insert(it, element);
    }
}

template <typename T>
bool erase_id(std::vector<T>& items, Id id)
{
    const auto it = std::lower_bound(items.begin(), items.end(), id, [](const T& e, Id v) { return e.id < v; });
    if (it != items.end() && it->id == id) {
        items.erase(it);
        return true;
    }
    return false;
}

} // namespace

Id element_id(const Element& element)
{
    return std::visit([](const auto& e) { return e.id; }, element);
}

void Schematic::set_next_id(Id next)
{
    Id highest = 0;
    for (const auto& c : components_) {
        highest = std::max(highest, c.id);
    }
    for (const auto& w : wires_) {
        highest = std::max(highest, w.id);
    }
    for (const auto& l : labels_) {
        highest = std::max(highest, l.id);
    }
    for (const auto& p : probes_) {
        highest = std::max(highest, p.id);
    }
    for (const auto& n : notes_) {
        highest = std::max(highest, n.id);
    }
    for (const auto& b : buses_) {
        highest = std::max(highest, b.id);
    }
    for (const auto& s : splitters_) {
        highest = std::max(highest, s.id);
    }
    next_id_ = std::max({next, highest + 1, next_id_});
}

const Component* Schematic::component(Id id) const
{
    return find(components_, id);
}

const Wire* Schematic::wire(Id id) const
{
    return find(wires_, id);
}

const NetLabel* Schematic::label(Id id) const
{
    return find(labels_, id);
}

const Probe* Schematic::probe(Id id) const
{
    return find(probes_, id);
}

const TextNote* Schematic::note(Id id) const
{
    return find(notes_, id);
}

const Bus* Schematic::bus(Id id) const
{
    return find(buses_, id);
}

const Splitter* Schematic::splitter(Id id) const
{
    return find(splitters_, id);
}

std::vector<SplitterPin> splitter_layout(const Splitter& splitter)
{
    std::vector<SplitterPin> pins;
    for (const bool left : {true, false}) {
        int bit = 0;
        int row = 0;
        for (const int width : left ? splitter.inputs : splitter.outputs) {
            pins.push_back({transform(Point{left ? 0 : 2, 2 * row}, splitter.position, splitter.orientation), width,
                left, bit});
            bit += width;
            ++row;
        }
    }
    return pins;
}

std::vector<Point> splitter_pins(const Splitter& splitter)
{
    std::vector<Point> points;
    for (const auto& pin : splitter_layout(splitter)) {
        points.push_back(pin.at);
    }
    return points;
}

std::optional<std::vector<int>> parse_widths(const std::string& text)
{
    std::vector<int> widths;
    int total = 0;
    std::string item;
    auto take = [&](std::string part) {
        part.erase(std::remove_if(part.begin(), part.end(), [](unsigned char c) { return std::isspace(c); }), part.end());
        if (part.empty()) {
            return false;
        }
        int repeat = 1;
        if (const auto star = part.find('*'); star != std::string::npos) {
            const std::string n = part.substr(0, star);
            if (n.empty() || n.size() > 2 || !std::all_of(n.begin(), n.end(), [](unsigned char c) { return std::isdigit(c); })) {
                return false;
            }
            repeat = std::stoi(n);
            part = part.substr(star + 1);
        }
        if (part.empty() || part.size() > 2
            || !std::all_of(part.begin(), part.end(), [](unsigned char c) { return std::isdigit(c); })) {
            return false;
        }
        const int width = std::stoi(part);
        if (width < 1 || repeat < 1) {
            return false;
        }
        for (int i = 0; i < repeat; ++i) {
            widths.push_back(width);
            total += width;
            if (total > 64) {
                return false;
            }
        }
        return true;
    };
    std::size_t start = 0;
    while (true) {
        const auto comma = text.find(',', start);
        if (!take(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start))) {
            return std::nullopt;
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return widths;
}

std::string widths_text(const std::vector<int>& widths)
{
    if (widths.size() > 1 && std::all_of(widths.begin(), widths.end(), [&](int w) { return w == widths.front(); })) {
        return std::to_string(widths.size()) + "*" + std::to_string(widths.front());
    }
    std::string text;
    for (const int w : widths) {
        text += (text.empty() ? "" : ",") + std::to_string(w);
    }
    return text;
}

const Component* Schematic::component_by_designator(const std::string& designator) const
{
    for (const auto& c : components_) {
        if (c.designator == designator) {
            return &c;
        }
    }
    return nullptr;
}

std::optional<Element> Schematic::get(Id id) const
{
    if (const auto* c = component(id)) {
        return *c;
    }
    if (const auto* w = wire(id)) {
        return *w;
    }
    if (const auto* l = label(id)) {
        return *l;
    }
    if (const auto* p = probe(id)) {
        return *p;
    }
    if (const auto* n = note(id)) {
        return *n;
    }
    if (const auto* b = bus(id)) {
        return *b;
    }
    if (const auto* s = splitter(id)) {
        return *s;
    }
    return std::nullopt;
}

void Schematic::put(const Element& element)
{
    // An ID belongs to one element kind; replacing across kinds would be a bug.
    const Id id = element_id(element);
    std::visit(
        [this, id](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (!std::is_same_v<T, Component>) {
                erase_id(components_, id);
            }
            if constexpr (!std::is_same_v<T, Wire>) {
                erase_id(wires_, id);
            }
            if constexpr (!std::is_same_v<T, NetLabel>) {
                erase_id(labels_, id);
            }
            if constexpr (!std::is_same_v<T, Probe>) {
                erase_id(probes_, id);
            }
            if constexpr (!std::is_same_v<T, TextNote>) {
                erase_id(notes_, id);
            }
            if constexpr (!std::is_same_v<T, Bus>) {
                erase_id(buses_, id);
            }
            if constexpr (!std::is_same_v<T, Splitter>) {
                erase_id(splitters_, id);
            }
            if constexpr (std::is_same_v<T, Component>) {
                put_sorted(components_, e);
            } else if constexpr (std::is_same_v<T, Wire>) {
                put_sorted(wires_, e);
            } else if constexpr (std::is_same_v<T, NetLabel>) {
                put_sorted(labels_, e);
            } else if constexpr (std::is_same_v<T, Probe>) {
                put_sorted(probes_, e);
            } else if constexpr (std::is_same_v<T, TextNote>) {
                put_sorted(notes_, e);
            } else if constexpr (std::is_same_v<T, Bus>) {
                put_sorted(buses_, e);
            } else {
                put_sorted(splitters_, e);
            }
        },
        element);
    if (id >= next_id_) {
        next_id_ = id + 1;
    }
}

void Schematic::erase(Id id)
{
    erase_id(components_, id) || erase_id(wires_, id) || erase_id(labels_, id) || erase_id(probes_, id)
        || erase_id(notes_, id) || erase_id(buses_, id) || erase_id(splitters_, id);
}

bool Schematic::empty() const
{
    return components_.empty() && wires_.empty() && labels_.empty() && probes_.empty() && notes_.empty()
        && buses_.empty() && splitters_.empty();
}

bool operator==(const Block& a, const Block& b)
{
    if (a.name != b.name || a.source != b.source) {
        return false;
    }
    if (a.schematic == b.schematic) {
        return true;
    }
    return a.schematic && b.schematic && *a.schematic == *b.schematic;
}

std::string block_type(const std::string& name)
{
    return "block:" + name;
}

std::string block_name(const std::string& type)
{
    return type.rfind("block:", 0) == 0 ? type.substr(6) : std::string();
}

const Block* Schematic::block(const std::string& name) const
{
    const auto it = blocks_.find(name);
    return it == blocks_.end() ? nullptr : &it->second;
}

std::string next_designator(const Schematic& schematic, const std::string& prefix)
{
    std::set<long long> used;
    for (const auto& c : schematic.components()) {
        if (c.designator.size() > prefix.size() && c.designator.compare(0, prefix.size(), prefix) == 0) {
            const std::string digits = c.designator.substr(prefix.size());
            if (std::all_of(digits.begin(), digits.end(), [](unsigned char ch) { return std::isdigit(ch); })
                && digits.size() < 12) {
                used.insert(std::stoll(digits));
            }
        }
    }
    long long n = 1;
    while (used.count(n)) {
        ++n;
    }
    return prefix + std::to_string(n);
}

} // namespace simnodus::model
