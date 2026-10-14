// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SPICE template lines of the component library:
//   {ref}        designator of the instance (R1)
//   {node:NAME}  net of pin NAME
//   {control}    node of the interactive control source
//   {NAME}       value of parameter NAME (a number in SPICE form, or the
//                chosen word for a parameter with choices)
//   {= EXPR}     arithmetic on parameters: + - * / parentheses, min(), max()
// Any other text is copied unchanged.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace simnodus::library {

struct TemplateToken {
    enum class Kind { Literal, Ref, Node, Parameter, Expression, Control };
    Kind kind = Kind::Literal;
    std::string text; // literal text, pin name, parameter name or expression
};

// Splits a template line; returns nullopt and sets `error` on bad syntax.
std::optional<std::vector<TemplateToken>> tokenize_template(std::string_view line, std::string& error);

using ValueLookup = std::function<std::optional<double>(std::string_view name)>;

// Evaluates a template expression. Unknown names, division by zero and
// syntax errors return nullopt with a message.
std::optional<double> evaluate_expression(std::string_view expression, const ValueLookup& lookup, std::string& error);

// Identifiers used by an expression (for validation at load time).
std::vector<std::string> expression_names(std::string_view expression);

} // namespace simnodus::library
