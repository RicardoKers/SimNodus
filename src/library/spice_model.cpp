// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "spice_model.hpp"

#include <algorithm>
#include <cctype>

namespace simnodus::library {

namespace {

std::string upper(std::string_view text)
{
    std::string result(text);
    for (auto& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

std::string trim(std::string_view text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

// Collapses runs of white space to one blank.
std::string collapse(std::string_view text)
{
    std::string result;
    bool blank = false;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            blank = !result.empty();
            continue;
        }
        if (blank) {
            result += ' ';
            blank = false;
        }
        result += c;
    }
    return result;
}

bool starts_with_word(std::string_view line, std::string_view word)
{
    if (line.size() < word.size() || upper(line.substr(0, word.size())) != word) {
        return false;
    }
    return line.size() == word.size() || std::isspace(static_cast<unsigned char>(line[word.size()]));
}

} // namespace

std::string SpiceModel::card(const std::string& as_name) const
{
    return ".model " + as_name + " " + type + "(" + parameters + ")";
}

bool supported_model_type(std::string_view type)
{
    const std::string t = upper(type);
    return t == "NPN" || t == "PNP" || t == "D" || t == "NMOS" || t == "PMOS";
}

std::optional<SpiceModel> parse_spice_model(std::string_view statement)
{
    const std::string line = collapse(statement);
    if (!starts_with_word(line, ".MODEL")) {
        return std::nullopt;
    }
    std::string_view rest(line);
    rest.remove_prefix(6);
    const std::string body = trim(rest);
    const auto name_end = body.find(' ');
    if (name_end == std::string::npos) {
        return std::nullopt;
    }
    SpiceModel model;
    model.name = body.substr(0, name_end);
    std::string tail = trim(std::string_view(body).substr(name_end + 1));
    const auto type_end = tail.find_first_of(" (");
    model.type = upper(tail.substr(0, type_end));
    if (!supported_model_type(model.type)) {
        return std::nullopt;
    }
    std::string parameters = type_end == std::string::npos ? std::string() : trim(std::string_view(tail).substr(type_end));
    if (!parameters.empty() && parameters.front() == '(') {
        const auto close = parameters.rfind(')');
        if (close == std::string::npos) {
            return std::nullopt; // unbalanced
        }
        parameters = trim(std::string_view(parameters).substr(1, close - 1));
    }
    // Model text is data for ngspice: no braces (parameter expressions), no
    // quotes and nothing that could end the statement early.
    if (parameters.find_first_of("{}\"';") != std::string::npos
        || std::any_of(model.name.begin(), model.name.end(),
            [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'); })) {
        return std::nullopt;
    }
    model.parameters = parameters;
    return model;
}

std::vector<SpiceModel> parse_spice_models(std::string_view text, int* skipped)
{
    std::vector<SpiceModel> models;
    int not_taken = 0;
    std::string statement;
    std::string comment;   // last comment line seen
    std::string described; // comment that belongs to the current statement
    bool in_subcircuit = false;
    auto finish = [&] {
        if (statement.empty()) {
            return;
        }
        if (starts_with_word(statement, ".MODEL")) {
            if (auto model = parse_spice_model(statement)) {
                model->description = described;
                models.push_back(std::move(*model));
            } else {
                ++not_taken;
            }
        }
        statement.clear();
    };
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        std::string line(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        start = end == std::string_view::npos ? text.size() + 1 : end + 1;
        if (const auto semicolon = line.find(';'); semicolon != std::string::npos) {
            line.erase(semicolon); // end-of-line comment
        }
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.front() == '*') {
            finish();
            comment = trim(std::string_view(trimmed).substr(1));
            continue;
        }
        if (trimmed.front() == '+') {
            if (!statement.empty()) {
                statement += ' ' + trimmed.substr(1);
            }
            continue;
        }
        finish();
        if (starts_with_word(trimmed, ".SUBCKT")) {
            in_subcircuit = true;
            ++not_taken;
        } else if (starts_with_word(trimmed, ".ENDS")) {
            in_subcircuit = false;
        } else if (!in_subcircuit && starts_with_word(trimmed, ".MODEL")) {
            statement = trimmed;
            described = comment;
        }
        comment.clear();
    }
    finish();
    if (skipped) {
        *skipped = not_taken;
    }
    return models;
}

} // namespace simnodus::library
