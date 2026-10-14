// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/declaration_ingress.hpp"
#include <cstdint>
#include <new>
#include <set>
#include <utility>

namespace simnodus {
namespace {
class Reader {
public:
    explicit Reader(std::string_view raw) { result.bytes = raw; }
    DeclarationSyntax read()
    {
        value(0);
        whitespace();
        if(pos != result.bytes.size()) fail(IngressErrorCode::syntax);
        return std::move(result);
    }
private:
    DeclarationSyntax result;
    std::size_t pos = 0;
    [[noreturn]] void fail(IngressErrorCode code) const { throw IngressError{code, pos}; }
    char peek() const { return pos < result.bytes.size() ? result.bytes[pos] : '\0'; }
    bool take(char c) { if(pos < result.bytes.size() && peek() == c) { ++pos; return true; } return false; }
    void expect(char c) { if(!take(c)) fail(IngressErrorCode::syntax); }
    void whitespace() { while(peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n') ++pos; }
    static bool digit(char c) { return c >= '0' && c <= '9'; }
    std::uint32_t hex4()
    {
        std::uint32_t code = 0;
        for(int i = 0; i < 4; ++i) {
            const auto c = peek();
            unsigned int n = 0;
            if(c >= '0' && c <= '9') n = static_cast<unsigned int>(c - '0');
            else if(c >= 'a' && c <= 'f') n = static_cast<unsigned int>(c - 'a' + 10);
            else if(c >= 'A' && c <= 'F') n = static_cast<unsigned int>(c - 'A' + 10);
            else fail(IngressErrorCode::syntax);
            ++pos;
            code = code * 16 + n;
        }
        return code;
    }
    static void utf8(std::string& text, std::uint32_t code)
    {
        if(code < 0x80) text += static_cast<char>(code);
        else if(code < 0x800) {
            text += static_cast<char>(0xc0 | (code >> 6));
            text += static_cast<char>(0x80 | (code & 63));
        } else if(code < 0x10000) {
            text += static_cast<char>(0xe0 | (code >> 12));
            text += static_cast<char>(0x80 | ((code >> 6) & 63));
            text += static_cast<char>(0x80 | (code & 63));
        } else {
            text += static_cast<char>(0xf0 | (code >> 18));
            text += static_cast<char>(0x80 | ((code >> 12) & 63));
            text += static_cast<char>(0x80 | ((code >> 6) & 63));
            text += static_cast<char>(0x80 | (code & 63));
        }
    }
    std::string string()
    {
        expect('"');
        std::string text;
        while(!take('"')) {
            if(pos == result.bytes.size()) fail(IngressErrorCode::syntax);
            const auto start = pos;
            const auto c = static_cast<unsigned char>(result.bytes[pos++]);
            if(c < 32) fail(IngressErrorCode::syntax);
            if(c == '\\') {
                const auto escape = peek();
                if(pos == result.bytes.size()) fail(IngressErrorCode::syntax);
                ++pos;
                switch(escape) {
                case '"': case '\\': case '/': text += escape; break;
                case 'b': text += '\b'; break;
                case 'f': text += '\f'; break;
                case 'n': text += '\n'; break;
                case 'r': text += '\r'; break;
                case 't': text += '\t'; break;
                case 'u': {
                    auto code = hex4();
                    if(code >= 0xd800 && code <= 0xdbff) {
                        if(!take('\\') || !take('u')) fail(IngressErrorCode::unicode);
                        const auto low = hex4();
                        if(low < 0xdc00 || low > 0xdfff) fail(IngressErrorCode::unicode);
                        code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                    } else if(code >= 0xdc00 && code <= 0xdfff) fail(IngressErrorCode::unicode);
                    utf8(text, code);
                    break;
                }
                default: fail(IngressErrorCode::syntax);
                }
            } else if(c < 128) text += static_cast<char>(c);
            else {
                int remaining = 0;
                std::uint32_t code = 0, minimum = 0;
                if(c >= 0xc2 && c <= 0xdf) { remaining = 1; code = c & 31; minimum = 0x80; }
                else if(c >= 0xe0 && c <= 0xef) { remaining = 2; code = c & 15; minimum = 0x800; }
                else if(c >= 0xf0 && c <= 0xf4) { remaining = 3; code = c & 7; minimum = 0x10000; }
                else fail(IngressErrorCode::unicode);
                for(int i = 0; i < remaining; ++i) {
                    if(pos == result.bytes.size()) fail(IngressErrorCode::unicode);
                    const auto next = static_cast<unsigned char>(result.bytes[pos]);
                    if((next & 0xc0) != 0x80) fail(IngressErrorCode::unicode);
                    code = (code << 6) | (next & 63);
                    ++pos;
                }
                if(code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
                    fail(IngressErrorCode::unicode);
                text.append(result.bytes, start, pos - start);
            }
        }
        return text;
    }
    void number()
    {
        take('-');
        if(!take('0')) {
            if(peek() < '1' || peek() > '9') fail(IngressErrorCode::syntax);
            while(digit(peek())) ++pos;
        }
        if(take('.')) {
            if(!digit(peek())) fail(IngressErrorCode::syntax);
            while(digit(peek())) ++pos;
        }
        if(take('e') || take('E')) {
            if(!take('+')) take('-');
            if(!digit(peek())) fail(IngressErrorCode::syntax);
            while(digit(peek())) ++pos;
        }
    }
    void literal(std::string_view text)
    {
        for(const auto c : text) expect(c);
    }
    void value(std::size_t depth)
    {
        whitespace();
        const auto index = result.tokens.size();
        JsonToken token{JsonKind::null, pos, 0, 0, {}};
        const auto c = peek();
        if(c == '{' || c == '[') {
            if(depth >= 32) fail(IngressErrorCode::depth);
            const bool object = c == '{';
            token.kind = object ? JsonKind::object : JsonKind::array;
            result.tokens.push_back(token);
            ++pos;
            whitespace();
            const char closing = object ? '}' : ']';
            std::set<std::string> keys;
            if(!take(closing)) {
                do {
                    whitespace();
                    if(object) {
                        const auto begin = pos;
                        auto key = string();
                        if(!keys.insert(key).second) fail(IngressErrorCode::duplicate_key);
                        result.tokens.push_back({JsonKind::string, begin, pos, result.tokens.size() + 1, std::move(key)});
                        whitespace();
                        expect(':');
                    }
                    value(depth + 1);
                    whitespace();
                    if(take(closing)) break;
                    expect(',');
                } while(true);
            }
        } else {
            switch(c) {
            case '"': token.kind = JsonKind::string; token.decoded = string(); break;
            case 't': token.kind = JsonKind::boolean; literal("true"); break;
            case 'f': token.kind = JsonKind::boolean; literal("false"); break;
            case 'n': literal("null"); break;
            default: token.kind = JsonKind::number; number(); break;
            }
            result.tokens.push_back(std::move(token));
        }
        result.tokens[index].end = pos;
        result.tokens[index].next = result.tokens.size();
    }
};
}
IngressResult capture_declaration_syntax(std::string_view bytes)
{
    if(bytes.size() > declaration_max_bytes) return IngressError{IngressErrorCode::bytes, declaration_max_bytes};
    try {
        return std::make_shared<const DeclarationSyntax>(Reader(bytes).read());
    } catch(const IngressError& error) { return error; }
    catch(const std::bad_alloc&) { return IngressError{IngressErrorCode::memory, 0}; }
}
}
