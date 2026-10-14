// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/managed_record.hpp"
#include "application/lock_digest_internal.hpp"
#include "application/project_validation.hpp"
#include "application/resource_path_policy_internal.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace simnodus::experimental {
namespace {
using Error = ManagedRecordError;
template<class T> bool zero(const T& value)
{
    return std::all_of(value.begin(), value.end(), [](auto b) { return b == 0; });
}
void require(bool condition, Error error)
{
    if(!condition) throw error;
}
struct Writer {
    std::string bytes;
    void raw(std::string_view value) { bytes.append(value); }
    template<std::size_t N> void array(const std::array<unsigned char, N>& value)
    {
        bytes.append(reinterpret_cast<const char*>(value.data()), N);
    }
    void integer(std::uint64_t value, unsigned count)
    {
        for(unsigned i = 0; i < count; ++i) bytes += static_cast<char>((value >> (8 * i)) & 255);
    }
};
struct Reader {
    std::string_view bytes;
    std::size_t position{};
    std::string_view raw(std::size_t count)
    {
        require(count <= bytes.size() - position, Error::size);
        auto result = bytes.substr(position, count);
        position += count;
        return result;
    }
    std::uint64_t integer(unsigned count)
    {
        const auto value = raw(count);
        std::uint64_t result = 0;
        for(unsigned i = 0; i < count; ++i)
            result |= static_cast<std::uint64_t>(static_cast<unsigned char>(value[i])) << (8 * i);
        return result;
    }
    std::uint32_t u32() { return static_cast<std::uint32_t>(integer(4)); }
    template<std::size_t N> std::array<unsigned char, N> array()
    {
        const auto value = raw(N);
        std::array<unsigned char, N> result{};
        std::copy(value.begin(), value.end(), result.begin());
        return result;
    }
};
ManagedDigest digest(std::string_view value)
{
    const auto hex = detail::managed_record_sha256(value);
    const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    ManagedDigest result{};
    for(std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<unsigned char>(16 * nibble(hex[2 * i]) + nibble(hex[2 * i + 1]));
    return result;
}
void identity(Writer& out, const ManagedIdentity& value)
{
    out.integer(value.volume, 8); out.array(value.file);
}
ManagedIdentity identity(Reader& in)
{
    return {in.integer(8), in.array<16>()};
}
void token(Writer& out, const ManagedToken& value)
{
    out.array(value.generation); out.array(value.document); out.integer(value.revision, 4);
    out.array(value.commit); out.array(value.digest); identity(out, value.identity);
}
ManagedToken token(Reader& in)
{
    ManagedToken result;
    result.generation = in.array<16>(); result.document = in.array<16>();
    result.revision = in.u32(); result.commit = in.array<16>();
    result.digest = in.array<32>(); result.identity = identity(in);
    return result;
}
bool utf8(std::string_view text)
{
    std::size_t i = 0;
    while(i < text.size()) {
        const auto first = static_cast<unsigned char>(text[i++]);
        if(first < 128) continue;
        unsigned remaining;
        std::uint32_t code, minimum;
        if(first >= 0xc2 && first <= 0xdf) { remaining = 1; code = first & 31; minimum = 0x80; }
        else if(first >= 0xe0 && first <= 0xef) { remaining = 2; code = first & 15; minimum = 0x800; }
        else if(first >= 0xf0 && first <= 0xf4) { remaining = 3; code = first & 7; minimum = 0x10000; }
        else return false;
        if(remaining > text.size() - i) return false;
        while(remaining--) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 63);
        }
        if(code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    return true;
}
void validate_context(const ManagedContext& value)
{
    require(!zero(value.binding) && !zero(value.identity.file) && value.policy == 1
        && value.locator.size() <= 4096 && utf8(value.locator)
        && resource_policy::root_syntax(value.locator), Error::context);
}
std::string context(const ManagedContext& value)
{
    validate_context(value);
    Writer out;
    out.raw("SRC1"); out.array(value.binding); identity(out, value.identity);
    out.integer(value.policy, 4); out.integer(value.locator.size(), 4); out.raw(value.locator);
    return out.bytes;
}
ManagedContext context(std::string_view bytes)
{
    Reader in{bytes};
    require(in.raw(4) == "SRC1", Error::context);
    ManagedContext result;
    result.binding = in.array<16>(); result.identity = identity(in); result.policy = in.u32();
    const auto size = in.u32();
    require(size <= 4096 && size == bytes.size() - in.position, Error::context);
    result.locator = in.raw(size);
    validate_context(result);
    return result;
}
void validate(const ManagedRecord& value)
{
    require(!zero(value.generation) && !zero(value.document) && !zero(value.commit)
        && !zero(value.operation) && value.revision >= 1 && value.revision <= managed_revision_limit,
        Error::fields);
    const auto& sid = value.principal_sid;
    require(sid.size() >= 12 && sid.size() <= 68 && sid[0] == 1, Error::fields);
    const auto count = static_cast<unsigned char>(sid[1]);
    require(count >= 1 && count <= 15 && sid.size() == 8u + 4u * count, Error::fields);
    const auto& parent = value.predecessor;
    if(value.revision == 1) require(parent == ManagedToken{}, Error::fields);
    else require(parent.generation == value.generation && parent.document == value.document
        && parent.revision == value.revision - 1 && !zero(parent.commit)
        && parent.commit != value.commit && !zero(parent.digest) && !zero(parent.identity.file), Error::fields);
    validate_context(value.context);
    require(!value.project.empty() && value.project.size() <= declaration_max_bytes, Error::size);
    require(std::holds_alternative<std::shared_ptr<const ProjectDeclaration>>(
        validate_project_declaration(value.project)), Error::project);
}
ManagedDigest request_digest(const ManagedRecord& value, std::string_view resource_context)
{
    // Commit ID is assigned by the writer, and is not part of the caller's request.
    // Execution-run fencing is separate from this stable operation digest.
    Writer out;
    out.raw("SNREQ001"); out.array(value.generation); out.array(value.document);
    out.array(value.operation); token(out, value.predecessor);
    out.integer(value.principal_sid.size(), 4); out.integer(resource_context.size(), 4);
    out.integer(value.project.size(), 4); out.raw(value.principal_sid);
    out.raw(resource_context); out.raw(value.project);
    return digest(out.bytes);
}
}
ManagedEncoding encode_managed_record(const ManagedRecord& value)
{
    try {
        validate(value);
        const auto resource_context = context(value.context);
        const auto size = 268 + value.principal_sid.size() + resource_context.size() + value.project.size();
        require(size <= managed_record_max_bytes, Error::size);
        Writer out;
        out.bytes.reserve(size);
        out.raw("SNMC0001"); out.integer(size, 4); out.integer(0, 4);
        out.integer(value.revision, 4); out.integer(value.principal_sid.size(), 4);
        out.integer(resource_context.size(), 4); out.integer(value.project.size(), 4);
        out.array(value.generation); out.array(value.document); out.array(value.commit); out.array(value.operation);
        token(out, value.predecessor); out.array(request_digest(value, resource_context));
        out.raw(value.principal_sid); out.raw(resource_context); out.raw(value.project);
        out.array(digest(out.bytes));
        return std::move(out.bytes);
    } catch(Error error) { return error; }
    catch(const std::bad_alloc&) { return Error::memory; }
    catch(const std::length_error&) { return Error::memory; }
}
ManagedDecoding decode_managed_record(std::string_view bytes)
{
    try {
        require(bytes.size() >= 268 && bytes.size() <= managed_record_max_bytes, Error::size);
        Reader in{bytes};
        require(in.raw(8) == "SNMC0001", Error::format);
        require(in.u32() == bytes.size(), Error::size);
        require(in.u32() == 0, Error::format);
        ManagedRecord value;
        value.revision = in.u32();
        const auto sid_size = in.u32(), context_size = in.u32(), project_size = in.u32();
        // Bound every count before any payload allocation or arithmetic summation.
        require(sid_size >= 12 && sid_size <= 68 && context_size >= 55
            && context_size <= managed_context_max_bytes && project_size >= 1
            && project_size <= declaration_max_bytes, Error::size);
        require(268u + sid_size + context_size + project_size == bytes.size(), Error::size);
        value.generation = in.array<16>(); value.document = in.array<16>();
        value.commit = in.array<16>(); value.operation = in.array<16>(); value.predecessor = token(in);
        const auto request = in.array<32>();
        value.principal_sid = in.raw(sid_size);
        const auto resource_context = in.raw(context_size);
        value.context = context(resource_context); value.project = in.raw(project_size);
        const auto recorded_digest = in.array<32>();
        require(recorded_digest == digest(bytes.substr(0, bytes.size() - 32)), Error::digest);
        validate(value);
        require(request == request_digest(value, resource_context), Error::digest);
        return DecodedManagedRecord{std::move(value), request, recorded_digest};
    } catch(Error error) { return error; }
    catch(const std::bad_alloc&) { return Error::memory; }
    catch(const std::length_error&) { return Error::memory; }
}
}
