// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SPICE device models of basic parts (PRJ-07, ADR 0095): `.model NAME TYPE(...)`
// statements read from model files (the library's built-in models and the
// user's models folder) and kept in projects. They are data: only `.model`
// statements of the types below are taken; subcircuits, other dot commands and
// anything else in a file are skipped.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace simnodus::library {

struct SpiceModel {
    std::string name;        // as written ("2N2222A")
    std::string type;        // upper case: NPN, PNP, D, NMOS, PMOS
    std::string parameters;  // the text inside the parentheses, on one line
    std::string description; // the comment line just before the statement, if any

    // The statement with another name, as emitted in a netlist.
    std::string card(const std::string& as_name) const;
    std::string card() const { return card(name); }
};

// Model types SimNodus accepts.
bool supported_model_type(std::string_view type);

// Every supported `.model` statement in `text` (`*` and `;` comments, `+`
// continuation lines, any letter case). `skipped`, when given, counts the
// statements that were not taken (subcircuits, unsupported types, broken lines).
std::vector<SpiceModel> parse_spice_models(std::string_view text, int* skipped = nullptr);

// One model from a single statement; nullopt when it is not a supported one.
std::optional<SpiceModel> parse_spice_model(std::string_view statement);

} // namespace simnodus::library
