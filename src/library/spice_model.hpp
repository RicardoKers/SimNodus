// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SPICE device models of basic parts (PRJ-07, ADR 0095): `.model NAME TYPE(...)`
// statements read from model files (the library's built-in models and the
// user's models folder) and kept in projects. They are data: only `.model`
// statements of the types below are taken; subcircuits, other dot commands and
// anything else in a file are skipped.
//
// Every parameter is checked before it can reach ngspice (ADR 0095): a value
// that is not a number (ngspice stops and must be restarted) or a name that is
// not a parameter of the model type (ngspice ignores it silently) is left out
// of the model and reported. Informational fields of vendor files (mfg, Vceo,
// Iave...) are left out without a report.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace simnodus::library {

struct SpiceModel {
    std::string name;        // as written ("2N2222A")
    std::string type;        // upper case: NPN, PNP, D, NMOS, PMOS
    std::string parameters;  // the accepted parameters, "NAME=VALUE" separated by blanks
    std::string description; // the comment line just before the statement, if any
    std::vector<std::string> dropped; // parameters left out, with the reason: "FOO=1 (not a D parameter)"

    // The statement with another name, as emitted in a netlist.
    std::string card(const std::string& as_name) const;
    std::string card() const { return card(name); }
};

// A statement of a model file that was skipped or changed.
struct ModelIssue {
    int line = 0;      // 1-based line where the statement starts
    std::string model; // model name when known
    std::string text;  // what happened
};

// A vendor subcircuit (`.subckt NAME PORTS ... .ends`, ADR 0097) used as the
// model of a part. `text` is self-contained: the file's `.model` statements
// and `.subckt` blocks it uses come first, its own block last. Commands that
// could read files or run commands (.include, .lib, .control...) are refused.
struct SpiceSubcircuit {
    std::string name;
    std::vector<std::string> ports; // as written: "1", "2", "99" ... or "IN+", "OUT"
    std::string description;        // the comments just before and after the header, one line each
    std::string text;
};

struct SpiceLibrary {
    std::vector<SpiceModel> models;
    std::vector<SpiceSubcircuit> subcircuits;
};

// Every supported `.model` statement and `.subckt` block of a model file;
// `issues` receives what was skipped or changed (see parse_spice_models).
SpiceLibrary parse_spice_library(std::string_view text, std::vector<ModelIssue>* issues = nullptr);

// The subcircuit a part keeps (the last block of `text`, which has its
// dependencies before it); nullopt when the text is not an acceptable one.
std::optional<SpiceSubcircuit> parse_subcircuit_text(std::string_view text, std::string* why = nullptr);

// Model types SimNodus accepts.
bool supported_model_type(std::string_view type);

// Every supported `.model` statement in `text` (`*` and `;` comments, `+`
// continuation lines, any letter case). `issues`, when given, receives the
// statements that were not taken (subcircuits, unsupported types, broken
// lines) and the parameters left out of the taken ones.
std::vector<SpiceModel> parse_spice_models(std::string_view text, std::vector<ModelIssue>* issues = nullptr);

// One model from a single statement; nullopt when it is not a supported one
// (`why`, when given, says why).
std::optional<SpiceModel> parse_spice_model(std::string_view statement, std::string* why = nullptr);

} // namespace simnodus::library
