// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "spice_model.hpp"

#include <algorithm>
#include <cctype>
#include <map>
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

namespace {

// One statement of a model file: its first line and its `+` continuations.
struct Statement {
    int line = 0;                   // first line, 1-based
    std::vector<std::string> lines; // as written (continuations included)
    std::string joined;             // continuations joined, end-of-line comments removed
    std::string comment;            // the comment line just before it
};

std::vector<Statement> statements_of(std::string_view text)
{
    std::vector<Statement> result;
    std::string comment;
    std::size_t start = 0;
    int line_number = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        std::string raw(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        start = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++line_number;
        if (!raw.empty() && raw.back() == '\r') {
            raw.pop_back();
        }
        std::string code = raw;
        if (const auto semicolon = code.find(';'); semicolon != std::string::npos) {
            code.erase(semicolon); // end-of-line comment
        }
        const std::string trimmed = trim(code);
        if (trimmed.empty()) {
            continue;
        }
        if (trimmed.front() == '*') {
            comment = trim(std::string_view(trimmed).substr(1));
            if (!result.empty() && result.back().lines.size() == 1
                && starts_with_word(result.back().joined, ".SUBCKT")) {
                result.back().lines.push_back(raw); // comments right after a header stay with the block
            }
            Statement c;
            c.line = line_number;
            c.lines = {raw};
            c.joined = "*" + comment;
            result.push_back(std::move(c));
            continue;
        }
        if (trimmed.front() == '+' && !result.empty() && result.back().joined.front() != '*') {
            result.back().lines.push_back(raw);
            result.back().joined += ' ' + trimmed.substr(1);
            continue;
        }
        Statement s;
        s.line = line_number;
        s.lines = {raw};
        s.joined = trimmed;
        s.comment = comment;
        comment.clear();
        result.push_back(std::move(s));
    }
    return result;
}

std::string first_word_upper(const std::string& statement)
{
    return upper(statement.substr(0, statement.find(' ')));
}

std::vector<std::string> words_of(const std::string& text)
{
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == ',' || c == '(' || c == ')') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

// A .subckt block of a file, with what it refers to.
struct Block {
    std::string name;
    std::vector<std::string> ports;
    std::vector<std::string> lines;
    std::string description;
    std::string refusal;            // why it cannot be used, empty when it can
    std::set<std::string> words;    // upper-case words of its element lines
    int line = 0;
};

// Dot commands an imported subcircuit may use; anything else could read files
// or run commands (.include, .lib, .control) or changes the whole simulation.
bool allowed_command(const std::string& word)
{
    return word == ".MODEL" || word == ".PARAM" || word == ".FUNC" || word == ".ENDS" || word == ".SUBCKT";
}

} // namespace

SpiceLibrary parse_spice_library(std::string_view text, std::vector<ModelIssue>* issues)
{
    SpiceLibrary library;
    auto report = [issues](int line, std::string model, std::string what) {
        if (issues) {
            issues->push_back({line, std::move(model), std::move(what)});
        }
    };
    const auto statements = statements_of(text);
    // File-level models (any type, for subcircuits) and the blocks.
    std::map<std::string, std::vector<std::string>> file_models; // upper-case name -> lines
    std::vector<std::string> model_order;
    std::map<std::string, std::pair<int, std::string>> unsupported; // name -> line, reason (until a block uses it)
    std::vector<Block> blocks;
    int depth = 0;
    for (const auto& s : statements) {
        if (s.joined.front() == '*') {
            if (depth > 0) {
                blocks.back().lines.insert(blocks.back().lines.end(), s.lines.begin(), s.lines.end());
            }
            continue;
        }
        const std::string word = first_word_upper(s.joined);
        if (word == ".SUBCKT") {
            if (depth == 0) {
                Block b;
                b.line = s.line;
                const auto words = words_of(collapse(s.joined));
                b.name = words.size() > 1 ? words[1] : "";
                for (std::size_t i = 2; i < words.size(); ++i) {
                    if (upper(words[i]) == "PARAMS:" || words[i].find('=') != std::string::npos) {
                        break;
                    }
                    b.ports.push_back(words[i]);
                }
                std::string description = s.comment;
                for (std::size_t i = 1; i < s.lines.size(); ++i) {
                    const std::string t = trim(s.lines[i]);
                    if (!t.empty() && t.front() == '*') {
                        const std::string c = trim(std::string_view(t).substr(1));
                        if (!c.empty() && c.find_first_not_of("*/=-_ ") != std::string::npos) {
                            description += (description.empty() ? "" : "\n") + c;
                        }
                    }
                }
                b.description = description;
                blocks.push_back(std::move(b));
            }
            ++depth;
            blocks.back().lines.insert(blocks.back().lines.end(), s.lines.begin(), s.lines.end());
            continue;
        }
        if (depth > 0) {
            Block& b = blocks.back();
            b.lines.insert(b.lines.end(), s.lines.begin(), s.lines.end());
            if (word.front() == '.') {
                if (!allowed_command(word) && b.refusal.empty()) {
                    b.refusal = "uses " + word + ", which imported models may not use";
                }
            } else if (!std::isalpha(static_cast<unsigned char>(word.front())) && b.refusal.empty()) {
                b.refusal = "line " + std::to_string(s.line) + " is not a SPICE element";
            } else {
                for (const auto& w : words_of(s.joined)) {
                    b.words.insert(upper(w));
                }
            }
            if (word == ".ENDS") {
                --depth;
            }
            continue;
        }
        if (word == ".MODEL") {
            const auto words = words_of(collapse(s.joined));
            if (words.size() > 1) {
                const std::string key = upper(words[1]);
                if (!file_models.count(key)) {
                    model_order.push_back(key);
                }
                file_models[key] = s.lines;
            }
            std::string why;
            if (auto model = parse_spice_model(s.joined, &why)) {
                model->description = s.comment;
                for (const auto& d : model->dropped) {
                    report(s.line, model->name, "left out " + d);
                }
                library.models.push_back(std::move(*model));
            } else if (why.find("is not supported") == std::string::npos) {
                report(s.line, words.size() > 1 ? words[1] : "", "skipped: " + why);
            } else if (words.size() > 1) {
                // Other types (NJF, RES, SW...) serve subcircuits; reported only if none uses them.
                unsupported[upper(words[1])] = {s.line, words[1] + "|skipped: " + why};
            }
        }
    }
    if (depth > 0 && !blocks.empty() && blocks.back().refusal.empty()) {
        blocks.back().refusal = ".ends is missing";
    }
    // Each usable block with the file's models and blocks it uses, transitively.
    std::map<std::string, std::size_t> block_index;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        block_index[upper(blocks[i].name)] = i;
        for (const auto& w : blocks[i].words) {
            unsupported.erase(w);
        }
    }
    for (const auto& [key, entry] : unsupported) {
        const auto bar = entry.second.find('|');
        report(entry.first, entry.second.substr(0, bar), entry.second.substr(bar + 1));
    }
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const Block& b = blocks[i];
        if (!b.refusal.empty()) {
            report(b.line, b.name, "skipped: " + b.refusal);
            continue;
        }
        std::set<std::size_t> needed_blocks;
        std::set<std::string> needed_models;
        std::vector<std::size_t> pending{i};
        std::string refusal;
        while (!pending.empty()) {
            const std::size_t k = pending.back();
            pending.pop_back();
            for (const auto& w : blocks[k].words) {
                if (file_models.count(w)) {
                    needed_models.insert(w);
                }
                if (const auto it = block_index.find(w); it != block_index.end() && it->second != k
                    && !needed_blocks.count(it->second)) {
                    if (!blocks[it->second].refusal.empty()) {
                        refusal = "uses " + blocks[it->second].name + ", which is skipped";
                    }
                    needed_blocks.insert(it->second);
                    pending.push_back(it->second);
                }
            }
        }
        needed_blocks.erase(i);
        if (!refusal.empty()) {
            report(b.line, b.name, "skipped: " + refusal);
            continue;
        }
        SpiceSubcircuit sub;
        sub.name = b.name;
        sub.ports = b.ports;
        sub.description = b.description;
        for (const auto& key : model_order) {
            if (needed_models.count(key)) {
                for (const auto& l : file_models[key]) {
                    sub.text += l + "\n";
                }
            }
        }
        for (const std::size_t k : needed_blocks) {
            for (const auto& l : blocks[k].lines) {
                sub.text += l + "\n";
            }
        }
        for (const auto& l : b.lines) {
            sub.text += l + "\n";
        }
        library.subcircuits.push_back(std::move(sub));
    }
    return library;
}

std::vector<SpiceModel> parse_spice_models(std::string_view text, std::vector<ModelIssue>* issues)
{
    return parse_spice_library(text, issues).models;
}

std::optional<SpiceSubcircuit> parse_subcircuit_text(std::string_view text, std::string* why)
{
    std::vector<ModelIssue> issues;
    auto library = parse_spice_library(text, &issues);
    for (const auto& issue : issues) {
        if (issue.text.rfind("skipped", 0) == 0) {
            if (why) {
                *why = issue.model + ": " + issue.text;
            }
            return std::nullopt;
        }
    }
    if (library.subcircuits.empty()) {
        if (why) {
            *why = "no .subckt block";
        }
        return std::nullopt;
    }
    SpiceSubcircuit top = library.subcircuits.back();
    top.text = std::string(text);
    return top;
}

} // namespace simnodus::library
