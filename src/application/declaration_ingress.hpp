// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace simnodus {
inline constexpr std::size_t declaration_max_bytes = 1024 * 1024;
enum class JsonKind { object, array, string, number, boolean, null };
struct JsonToken {
    JsonKind kind;
    std::size_t begin, end, next;
    std::string decoded;
};
struct DeclarationSyntax {
    std::string bytes;
    std::vector<JsonToken> tokens;
};
enum class IngressErrorCode { bytes, syntax, unicode, duplicate_key, depth, memory };
struct IngressError { IngressErrorCode code; std::size_t offset; };
using IngressResult = std::variant<std::shared_ptr<const DeclarationSyntax>, IngressError>;
// Syntax only. Borrowed input must remain stable for the duration of this call.
IngressResult capture_declaration_syntax(std::string_view bytes);
}
