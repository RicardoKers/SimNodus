// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "spice_template.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>

namespace simnodus::library {
namespace {

std::string trimmed(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return std::string(s);
}

bool identifier_start(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool identifier_char(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

// Recursive-descent parser: expr := term (('+'|'-') term)*,
// term := factor (('*'|'/') factor)*, factor := number | name | call | '(' expr ')' | '-' factor.
class Parser {
public:
    Parser(std::string_view text, const ValueLookup& lookup, std::string& error)
        : text_(text), lookup_(lookup), error_(error)
    {
    }

    std::optional<double> parse()
    {
        auto value = expression();
        skip_space();
        if (value && pos_ != text_.size()) {
            return fail("unexpected '" + std::string(1, text_[pos_]) + "'");
        }
        return value;
    }

private:
    std::optional<double> fail(const std::string& message)
    {
        if (error_.empty()) {
            error_ = message;
        }
        return std::nullopt;
    }

    void skip_space()
    {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool accept(char c)
    {
        skip_space();
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    std::optional<double> expression()
    {
        auto left = term();
        while (left) {
            if (accept('+')) {
                auto right = term();
                if (!right) {
                    return std::nullopt;
                }
                *left += *right;
            } else if (accept('-')) {
                auto right = term();
                if (!right) {
                    return std::nullopt;
                }
                *left -= *right;
            } else {
                break;
            }
        }
        return left;
    }

    std::optional<double> term()
    {
        auto left = factor();
        while (left) {
            if (accept('*')) {
                auto right = factor();
                if (!right) {
                    return std::nullopt;
                }
                *left *= *right;
            } else if (accept('/')) {
                auto right = factor();
                if (!right) {
                    return std::nullopt;
                }
                if (*right == 0.0) {
                    return fail("division by zero");
                }
                *left /= *right;
            } else {
                break;
            }
        }
        return left;
    }

    std::optional<double> factor()
    {
        skip_space();
        if (accept('-')) {
            auto value = factor();
            return value ? std::optional<double>(-*value) : std::nullopt;
        }
        if (accept('(')) {
            auto value = expression();
            if (!value) {
                return std::nullopt;
            }
            if (!accept(')')) {
                return fail("missing ')'");
            }
            return value;
        }
        skip_space();
        if (pos_ >= text_.size()) {
            return fail("unexpected end of expression");
        }
        if (identifier_start(text_[pos_])) {
            const std::size_t begin = pos_;
            while (pos_ < text_.size() && identifier_char(text_[pos_])) {
                ++pos_;
            }
            const std::string name(text_.substr(begin, pos_ - begin));
            if (name == "min" || name == "max" || name == "mod") {
                if (!accept('(')) {
                    return fail("expected '(' after " + name);
                }
                auto a = expression();
                if (!a || !accept(',')) {
                    return a ? fail("expected ',' in " + name) : std::nullopt;
                }
                auto b = expression();
                if (!b || !accept(')')) {
                    return b ? fail("missing ')'") : std::nullopt;
                }
                if (name == "mod") {
                    if (*b == 0.0) {
                        return fail("mod by zero");
                    }
                    return *a - *b * std::floor(*a / *b); // result has the sign of b
                }
                return name == "min" ? std::min(*a, *b) : std::max(*a, *b);
            }
            auto value = lookup_ ? lookup_(name) : std::nullopt;
            if (!value) {
                return fail("unknown value '" + name + "'");
            }
            return value;
        }
        // std::from_chars is independent of the C locale (Qt sets it on Linux).
        double value = 0.0;
        const char* begin = text_.data() + pos_;
        const auto [end, status] = std::from_chars(begin, text_.data() + text_.size(), value);
        if (status != std::errc() || end == begin) {
            return fail("expected a number");
        }
        pos_ += static_cast<std::size_t>(end - begin);
        return value;
    }

    std::string_view text_;
    const ValueLookup& lookup_;
    std::string& error_;
    std::size_t pos_ = 0;
};

} // namespace

std::optional<std::vector<TemplateToken>> tokenize_template(std::string_view line, std::string& error)
{
    std::vector<TemplateToken> tokens;
    std::string literal;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '}') {
            error = "unmatched '}'";
            return std::nullopt;
        }
        if (line[i] != '{') {
            literal += line[i];
            continue;
        }
        const std::size_t close = line.find('}', i + 1);
        if (close == std::string_view::npos) {
            error = "unmatched '{'";
            return std::nullopt;
        }
        if (!literal.empty()) {
            tokens.push_back({TemplateToken::Kind::Literal, literal});
            literal.clear();
        }
        const std::string inside = trimmed(line.substr(i + 1, close - i - 1));
        if (inside.empty()) {
            error = "empty placeholder";
            return std::nullopt;
        }
        if (inside == "ref") {
            tokens.push_back({TemplateToken::Kind::Ref, {}});
        } else if (inside == "control") {
            tokens.push_back({TemplateToken::Kind::Control, {}});
        } else if (inside.rfind("ext:", 0) == 0) {
            const std::string name = trimmed(std::string_view(inside).substr(4));
            if (name.empty() || !std::all_of(name.begin(), name.end(), identifier_char)) {
                error = "invalid external source name in {" + inside + "}";
                return std::nullopt;
            }
            tokens.push_back({TemplateToken::Kind::External, name});
        } else if (inside.rfind("node:", 0) == 0) {
            tokens.push_back({TemplateToken::Kind::Node, trimmed(std::string_view(inside).substr(5))});
        } else if (inside.front() == '=') {
            tokens.push_back({TemplateToken::Kind::Expression, trimmed(std::string_view(inside).substr(1))});
        } else {
            for (char c : inside) {
                if (!identifier_char(c)) {
                    error = "invalid placeholder {" + inside + "}";
                    return std::nullopt;
                }
            }
            tokens.push_back({TemplateToken::Kind::Parameter, inside});
        }
        i = close;
    }
    if (!literal.empty()) {
        tokens.push_back({TemplateToken::Kind::Literal, literal});
    }
    return tokens;
}

std::optional<double> evaluate_expression(std::string_view expression, const ValueLookup& lookup, std::string& error)
{
    error.clear();
    Parser parser(expression, lookup, error);
    auto value = parser.parse();
    if (value && !std::isfinite(*value)) {
        error = "result is not finite";
        return std::nullopt;
    }
    return value;
}

std::vector<std::string> expression_names(std::string_view expression)
{
    std::vector<std::string> names;
    for (std::size_t i = 0; i < expression.size();) {
        if (identifier_start(expression[i]) && (i == 0 || !identifier_char(expression[i - 1]))) {
            // Skip exponents such as the "e" in 1e-3.
            if ((expression[i] == 'e' || expression[i] == 'E') && i > 0
                && (std::isdigit(static_cast<unsigned char>(expression[i - 1])) || expression[i - 1] == '.')) {
                ++i;
                continue;
            }
            std::size_t end = i;
            while (end < expression.size() && identifier_char(expression[end])) {
                ++end;
            }
            std::string name(expression.substr(i, end - i));
            if (name != "min" && name != "max" && name != "mod") {
                names.push_back(name);
            }
            i = end;
        } else {
            ++i;
        }
    }
    return names;
}

} // namespace simnodus::library
