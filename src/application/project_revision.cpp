// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_revision.hpp"
#include <new>

namespace simnodus {
ProjectRevisionResult rename_project(std::string_view original, std::string_view name)
{
    auto stage = ProjectRevisionStage::base;
    try {
        const auto base = load_project_graph(original);
        if(const auto* error = std::get_if<LockError>(&base))
            return ProjectRevisionError{stage, error->code, error->offset};
        stage = ProjectRevisionStage::revision;
        const auto& syntax = *std::get<0>(base)->declaration->sources.lock.syntax;
        // Complete base validation guarantees exactly one top-level string name.
        std::size_t selected = 0;
        for(std::size_t key = 1; key < syntax.tokens[0].next; key = syntax.tokens[key + 1].next)
            if(syntax.tokens[key].decoded == "name") { selected = key + 1; break; }
        const auto& token = syntax.tokens[selected];
        if(token.decoded == name) return std::get<0>(base);
        // The accepted name has at most 80 Unicode scalars, hence <=320 UTF-8
        // bytes. Full scalar/control validation stays in the unchanged validator.
        if(name.size() > 320) return ProjectRevisionError{stage, "value", token.begin};
        std::string encoded = "\"";
        for(unsigned char c : name) {
            if(c == '"' || c == '\\') { encoded += '\\'; encoded += static_cast<char>(c); }
            else if(c < 32) {
                encoded += "\\u00";
                encoded += "0123456789abcdef"[c >> 4];
                encoded += "0123456789abcdef"[c & 15];
            } else encoded += static_cast<char>(c);
        }
        encoded += '"';
        const auto retained = syntax.bytes.size() - (token.end - token.begin);
        if(encoded.size() > declaration_max_bytes - retained)
            return ProjectRevisionError{stage, "bytes", 0};
        auto revised = syntax.bytes.substr(0, token.begin);
        revised += encoded;
        revised.append(syntax.bytes, token.end, std::string::npos);
        const auto result = load_project_graph(revised);
        if(const auto* error = std::get_if<LockError>(&result))
            return ProjectRevisionError{stage, error->code, error->offset};
        return std::get<0>(result);
    } catch(const std::bad_alloc&) { return ProjectRevisionError{stage, "memory", 0}; }
}
}
