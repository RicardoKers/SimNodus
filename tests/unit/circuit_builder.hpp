// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Test helper: builds schematics by placing components far apart and putting
// net labels on their pins, so tests describe circuits by net names.
#pragma once

#include "editing/operations.hpp"
#include "library/component_library.hpp"
#include "model/schematic.hpp"

#include <map>
#include <string>

namespace simnodus::test {

class CircuitBuilder {
public:
    explicit CircuitBuilder(const library::ComponentLibrary& library) : library_(library) {}

    // Places `type` and labels each listed pin with a net name ("0" is ground).
    model::Id add(const std::string& type, const std::map<std::string, std::string>& pin_nets,
        const std::map<std::string, std::string>& parameters = {})
    {
        const auto* d = library_.find(type);
        if (!d) {
            return model::no_id;
        }
        model::Id id = model::no_id;
        editing::add_component(schematic, *d, {next_x_, 0}, {}, &id).apply(schematic);
        next_x_ += 20;
        for (const auto& [name, value] : parameters) {
            editing::set_parameter(schematic, id, name, value).apply(schematic);
        }
        const auto* c = schematic.component(id);
        for (const auto& [pin, net] : pin_nets) {
            if (const auto* p = d->pin(pin)) {
                editing::add_label(schematic, library::pin_position(*d, *p, *c), net).apply(schematic);
            }
        }
        return id;
    }

    model::Schematic schematic;

private:
    const library::ComponentLibrary& library_;
    int next_x_ = 0;
};

} // namespace simnodus::test
