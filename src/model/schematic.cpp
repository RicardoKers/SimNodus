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
            if constexpr (std::is_same_v<T, Component>) {
                put_sorted(components_, e);
            } else if constexpr (std::is_same_v<T, Wire>) {
                put_sorted(wires_, e);
            } else if constexpr (std::is_same_v<T, NetLabel>) {
                put_sorted(labels_, e);
            } else {
                put_sorted(probes_, e);
            }
        },
        element);
    if (id >= next_id_) {
        next_id_ = id + 1;
    }
}

void Schematic::erase(Id id)
{
    erase_id(components_, id) || erase_id(wires_, id) || erase_id(labels_, id) || erase_id(probes_, id);
}

bool Schematic::empty() const
{
    return components_.empty() && wires_.empty() && labels_.empty() && probes_.empty();
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
