// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "spice_model.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

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

// Parameter names ngspice knows (ngspice manual: diode, Gummel-Poon BJT with
// its temperature coefficients, MOSFET levels 1-3), upper case.
const std::set<std::string>& diode_parameters()
{
    static const std::set<std::string> names{"IS", "JS", "JSW", "N", "RS", "TT", "CJO", "CJ0", "CJ", "VJ", "PB", "M",
        "MJ", "CJSW", "CJP", "VJSW", "PHP", "MJSW", "FC", "FCS", "EG", "XTI", "KF", "AF", "BV", "IBV", "IB", "NBV",
        "IBVL", "NBVL", "IKF", "IK", "IKR", "ISR", "NR", "TNOM", "TREF", "LEVEL", "RSW", "TCV", "TRS", "TRS1", "TRS2",
        "TBV1", "TBV2", "TTT1", "TTT2", "TM1", "TM2", "TLEV", "TLEVC", "CTA", "CTP", "TPB", "TPHP", "AREA", "PJ",
        "LM", "LP", "WM", "WP", "XOM", "XOI", "XM", "XP", "SHRINK"};
    return names;
}

const std::set<std::string>& bjt_parameters()
{
    static const std::set<std::string> names{"IS", "IBE", "IBC", "BF", "NF", "VAF", "VA", "IKF", "IK", "ISE", "NE",
        "BR", "NR", "VAR", "VB", "IKR", "ISC", "NC", "RB", "IRB", "RBM", "RE", "RC", "CJE", "VJE", "PE", "MJE", "ME",
        "TF", "XTF", "VTF", "ITF", "PTF", "CJC", "VJC", "PC", "MJC", "MC", "XCJC", "TR", "CJS", "CCS", "VJS", "PS",
        "MJS", "MS", "XTB", "EG", "XTI", "KF", "AF", "FC", "TNOM", "TREF", "LEVEL", "SUBS", "NKF", "ISS", "NS", "QCO",
        "RCO", "VO", "GAMMA", "QUASIMOD", "VG", "CN", "TLEV", "TLEVC", "TBF1", "TBF2", "TBR1", "TBR2", "TIKF1",
        "TIKF2", "TIKR1", "TIKR2", "TIRB1", "TIRB2", "TNC1", "TNC2", "TNE1", "TNE2", "TNF1", "TNF2", "TNR1", "TNR2",
        "TRB1", "TRB2", "TRC1", "TRC2", "TRE1", "TRE2", "TRM1", "TRM2", "TVAF1", "TVAF2", "TVAR1", "TVAR2", "CTC",
        "CTE", "CTS", "TVJC", "TVJE", "TVJS", "TITF1", "TITF2", "TTF1", "TTF2", "TTR1", "TTR2", "TMJE1", "TMJE2",
        "TMJC1", "TMJC2", "TMJS1", "TMJS2", "TISE1", "TISE2", "TISC1", "TISC2", "TIS1", "TIS2", "TISS1", "TISS2",
        "TNS1", "TNS2", "TNKF1", "TNKF2", "TVO1", "TVO2", "TBV1", "TBV2"};
    return names;
}

const std::set<std::string>& mos_parameters()
{
    static const std::set<std::string> names{"LEVEL", "VTO", "VT0", "KP", "GAMMA", "PHI", "LAMBDA", "RD", "RS", "CBD",
        "CBS", "IS", "PB", "CGSO", "CGDO", "CGBO", "RSH", "CJ", "MJ", "CJSW", "MJSW", "JS", "JSW", "TOX", "NSUB",
        "NSS", "NFS", "TPG", "XJ", "LD", "WD", "UO", "U0", "UCRIT", "UEXP", "UTRA", "VMAX", "NEFF", "KF", "AF", "FC",
        "DELTA", "THETA", "ETA", "KAPPA", "TNOM", "TREF", "XL", "XW", "DELVTO", "XQC", "FNCOEF", "FNEXP"};
    return names;
}

// Fields of vendor and LTspice model files that describe the part and are
// not simulation parameters; ngspice does not know them.
bool informational(const std::string& name)
{
    static const std::set<std::string> names{"MFG", "TYPE", "IAVE", "VPK", "ICRATING", "VCEO", "VDS", "RON", "QG",
        "VCBO", "VEBO", "IC", "PD", "BVCEO", "MODEL", "DESC"};
    return names.count(name) > 0;
}

bool number(const std::string& value)
{
    // SPICE number: digits, optional fraction and exponent, then letters
    // (scale factor and unit: 10p, 4.7k, 1meg, 0.5V).
    static const std::regex pattern(R"(^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?[a-zA-Z]*$)");
    return std::regex_match(value, pattern);
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

std::optional<SpiceModel> parse_spice_model(std::string_view statement, std::string* why)
{
    auto refuse = [why](std::string reason) -> std::optional<SpiceModel> {
        if (why) {
            *why = std::move(reason);
        }
        return std::nullopt;
    };
    const std::string line = collapse(statement);
    if (!starts_with_word(line, ".MODEL")) {
        return refuse("not a .model statement");
    }
    std::string_view rest(line);
    rest.remove_prefix(6);
    const std::string body = trim(rest);
    const auto name_end = body.find(' ');
    if (name_end == std::string::npos) {
        return refuse("no model type");
    }
    SpiceModel model;
    model.name = body.substr(0, name_end);
    if (std::any_of(model.name.begin(), model.name.end(),
            [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'); })) {
        return refuse("the name may only have letters, digits, '_', '-' and '.'");
    }
    const std::string tail = trim(std::string_view(body).substr(name_end + 1));
    const auto type_end = tail.find_first_of(" (");
    model.type = upper(tail.substr(0, type_end));
    if (!supported_model_type(model.type)) {
        return refuse("type " + model.type + " is not supported (NPN, PNP, D, NMOS, PMOS)");
    }
    std::string parameters = type_end == std::string::npos ? std::string() : trim(std::string_view(tail).substr(type_end));
    if (!parameters.empty() && parameters.front() == '(') {
        const auto close = parameters.rfind(')');
        if (close == std::string::npos) {
            return refuse("a parenthesis is not closed");
        }
        parameters = parameters.substr(1, close - 1); // text after the parameters is dropped
    }
    // NAME=VALUE pairs; a value may be quoted or an expression in braces.
    static const std::regex pair(R"(([A-Za-z_][A-Za-z0-9_]*)\s*=\s*("[^"]*"|'[^']*'|\{[^}]*\}|[^\s,()]+))");
    const std::set<std::string>* known = model.type == "D" ? &diode_parameters()
        : (model.type == "NPN" || model.type == "PNP") ? &bjt_parameters()
                                                       : &mos_parameters();
    std::vector<std::pair<std::string, std::string>> pairs;
    for (auto it = std::sregex_iterator(parameters.begin(), parameters.end(), pair); it != std::sregex_iterator(); ++it) {
        pairs.emplace_back((*it)[1].str(), (*it)[2].str());
    }
    // MOSFET levels above 3 (BSIM...) have hundreds of parameters: names are not checked there.
    bool check_names = true;
    for (const auto& [name, value] : pairs) {
        if (upper(name) == "LEVEL" && number(value)) {
            check_names = !(model.type == "NMOS" || model.type == "PMOS") || std::stod(value) <= 3;
        }
    }
    std::string accepted;
    for (const auto& [name, value] : pairs) {
        const std::string key = upper(name);
        if (informational(key) && !known->count(key)) {
            continue; // describes the part; not a simulation parameter
        }
        if (!number(value)) {
            model.dropped.push_back(name + "=" + value + " (not a number)");
        } else if (check_names && !known->count(key)) {
            model.dropped.push_back(name + "=" + value + " (not a " + model.type + " parameter)");
        } else {
            accepted += (accepted.empty() ? "" : " ") + name + "=" + value;
        }
    }
    model.parameters = accepted;
    return model;
}

std::vector<SpiceModel> parse_spice_models(std::string_view text, std::vector<ModelIssue>* issues)
{
    std::vector<SpiceModel> models;
    std::string statement;
    int statement_line = 0;
    std::string comment;   // last comment line seen
    std::string described; // comment that belongs to the current statement
    int subcircuit_line = 0;
    std::string subcircuit;
    auto report = [issues](int line, std::string model, std::string text) {
        if (issues) {
            issues->push_back({line, std::move(model), std::move(text)});
        }
    };
    auto finish = [&] {
        if (statement.empty()) {
            return;
        }
        std::string why;
        if (auto model = parse_spice_model(statement, &why)) {
            model->description = described;
            for (const auto& d : model->dropped) {
                report(statement_line, model->name, "left out " + d);
            }
            models.push_back(std::move(*model));
        } else {
            const std::string words = collapse(statement);
            const auto name_start = words.find(' ');
            const auto name_end = words.find(' ', name_start + 1);
            const std::string name =
                name_start == std::string::npos ? std::string() : words.substr(name_start + 1, name_end - name_start - 1);
            report(statement_line, name, "skipped: " + why);
        }
        statement.clear();
    };
    std::size_t start = 0;
    int line_number = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        std::string line(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        start = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++line_number;
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
            const std::string words = collapse(trimmed);
            const auto name_end = words.find(' ', 8);
            subcircuit = words.size() > 8 ? words.substr(8, name_end == std::string::npos ? std::string::npos : name_end - 8) : "";
            subcircuit_line = line_number;
        } else if (starts_with_word(trimmed, ".ENDS")) {
            if (subcircuit_line > 0) {
                report(subcircuit_line, subcircuit, "skipped: subcircuits (.subckt) are not supported yet");
            }
            subcircuit_line = 0;
        } else if (subcircuit_line == 0 && starts_with_word(trimmed, ".MODEL")) {
            statement = trimmed;
            statement_line = line_number;
            described = comment;
        }
        comment.clear();
    }
    finish();
    return models;
}

} // namespace simnodus::library
