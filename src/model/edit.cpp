// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "edit.hpp"

#include <algorithm>

namespace simnodus::model {

void Edit::apply(Schematic& schematic) const
{
    for (const auto& change : changes_) {
        if (change.after) {
            schematic.put(*change.after);
        } else {
            schematic.erase(change.id);
        }
    }
    if (settings_) {
        schematic.set_settings(settings_->second);
    }
}

void Edit::revert(Schematic& schematic) const
{
    for (auto it = changes_.rbegin(); it != changes_.rend(); ++it) {
        if (it->before) {
            schematic.put(*it->before);
        } else {
            schematic.erase(it->id);
        }
    }
    if (settings_) {
        schematic.set_settings(settings_->first);
    }
}

void Edit::set_settings(const SimulationSettings& before, const SimulationSettings& after)
{
    if (before == after) {
        settings_.reset();
    } else {
        settings_ = std::make_pair(before, after);
    }
}

Edit make_edit(const Schematic& before, const Schematic& after, std::string description)
{
    Edit edit(std::move(description));
    std::vector<Id> ids;
    auto collect = [&ids](const Schematic& s) {
        for (const auto& e : s.components()) {
            ids.push_back(e.id);
        }
        for (const auto& e : s.wires()) {
            ids.push_back(e.id);
        }
        for (const auto& e : s.labels()) {
            ids.push_back(e.id);
        }
        for (const auto& e : s.probes()) {
            ids.push_back(e.id);
        }
        for (const auto& e : s.notes()) {
            ids.push_back(e.id);
        }
    };
    collect(before);
    collect(after);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    for (Id id : ids) {
        auto old_element = before.get(id);
        auto new_element = after.get(id);
        if (old_element != new_element) {
            edit.changes_.push_back({id, std::move(old_element), std::move(new_element)});
        }
    }
    edit.set_settings(before.settings(), after.settings());
    return edit;
}

} // namespace simnodus::model
