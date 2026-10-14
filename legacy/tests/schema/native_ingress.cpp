// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/declaration_ingress.hpp"
#include <iostream>
#include <type_traits>

int main()
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*std::get<0>(IngressResult{}))>>);
    int cases = 0, failures = 0;
    const auto check = [&](bool ok) { ++cases; if(!ok) { ++failures; std::cerr << "Failed case " << cases << '\n'; } };
    const auto good = [&](const std::string& raw) {
        const auto result = capture_declaration_syntax(raw);
        check(std::holds_alternative<std::shared_ptr<const DeclarationSyntax>>(result));
    };
    const auto bad = [&](const std::string& raw, IngressErrorCode code) {
        const auto result = capture_declaration_syntax(raw);
        const auto* error = std::get_if<IngressError>(&result);
        check(error && error->code == code && error->offset <= raw.size());
    };
    std::string raw = " {\"key\":[18446744073709551615,-0,1.00e+2,true,false,null,\"\\u0061\"]} \n";
    auto result = capture_declaration_syntax(raw);
    if(!std::holds_alternative<std::shared_ptr<const DeclarationSyntax>>(result)) return 1;
    const auto syntax = std::get<0>(result);
    check(syntax->bytes == raw);
    raw.assign("changed input");
    check(syntax->bytes != raw);
    check(syntax->tokens.size() == 10);
    check(syntax->tokens[0].kind == JsonKind::object && syntax->tokens[0].next == 10);
    check(syntax->tokens[1].decoded == "key" && syntax->tokens[1].next == 2);
    check(syntax->tokens[2].kind == JsonKind::array && syntax->tokens[2].next == 10);
    for(std::size_t i = 3; i <= 9; ++i) check(syntax->tokens[i].next == i + 1);
    const auto text = [&](std::size_t i) { const auto& t = syntax->tokens[i]; return syntax->bytes.substr(t.begin, t.end - t.begin); };
    check(text(3) == "18446744073709551615");
    check(text(4) == "-0");
    check(text(5) == "1.00e+2");
    check(syntax->tokens[9].decoded == "a" && text(9) == "\"\\u0061\"");
    good(std::string(32, '[') + "0" + std::string(32, ']'));
    bad(std::string(33, '[') + "0" + std::string(33, ']'), IngressErrorCode::depth);
    good("0" + std::string(declaration_max_bytes - 1, ' '));
    bad("0" + std::string(declaration_max_bytes, ' '), IngressErrorCode::bytes);
    good("\"" + std::string(declaration_max_bytes - 2, 'a') + "\"");
    good("[1e9999,-1e-9999,18446744073709551616]");
    good("{\"x\":{},\"y\":{\"x\":[]}}");
    good("[\"\\ud800\\udc00\",\"\\udbff\\udfff\",\"\\u0000\"]");
    bad("{\"a\":1,\"\\u0061\":2}", IngressErrorCode::duplicate_key);
    bad("\"\\ud800\"", IngressErrorCode::unicode);
    bad("\"\\udc00\"", IngressErrorCode::unicode);
    for(const std::string input : {"", " ", "01", "+1", "1.", "1e", "NaN", "Infinity", "[1,]", "{\"a\":}", "{}[]", "/*x*/{}", "\"\\x01\""})
        bad(input, IngressErrorCode::syntax);
    for(const std::string bytes : {"\xc0\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\x80", "\xe2\x82"})
        bad("\"" + bytes + "\"", IngressErrorCode::unicode);
    const std::string complete = "{\"x\":[true,false,null,\"\\uD834\\uDD1E\",-12.5e+3]}";
    for(std::size_t i = 0; i < complete.size(); ++i) {
        const auto partial = capture_declaration_syntax(std::string_view(complete).substr(0, i));
        check(std::holds_alternative<IngressError>(partial));
    }
    // Maximum breadth within the byte budget must not depend on call-stack depth.
    std::string broad = "[0";
    for(std::size_t i = 1; i < declaration_max_bytes / 2 - 1; ++i) broad += ",0";
    broad += ']';
    good(broad);
    const auto nul_keys = capture_declaration_syntax("{\"\\u0000\":0,\"\\u0000\":1}");
    check(std::holds_alternative<IngressError>(nul_keys)
        && std::get<IngressError>(nul_keys).code == IngressErrorCode::duplicate_key);
    std::cout << cases << " native ingress assertions, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
