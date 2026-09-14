// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/lock_validation.hpp"
#include "application/lock_digest_internal.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <new>
#include <set>

namespace simnodus {
namespace {
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
bool digit(char c) { return c >= '0' && c <= '9'; }
bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
std::string lower(std::string value)
{
    for(auto& c : value) if(c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}
class Validator {
public:
    explicit Validator(std::shared_ptr<const DeclarationSyntax> syntax) : syntax_(std::move(syntax)) {}
    LockDeclaration run()
    {
        const auto root = fields(0, {"format", "version", "dependencies"});
        require(string(root.at("format"), "version") == "simnodus-resource-lock"
            && string(root.at("version"), "version") == "0.1", "version", 0);
        const auto dependencies = array(root.at("dependencies"));
        require(dependencies.size() <= 32, "budget", root.at("dependencies"));
        LockDeclaration result{syntax_, dependencies.size(), 0, {}, {}};
        std::set<std::string> ids, paths;
        for(const auto token : dependencies) {
            const auto dependency = fields(token, {"id", "version", "origin", "license", "files", "content_sha256"});
            const auto id = identifier(dependency.at("id"));
            require(ids.insert(id).second, "id", dependency.at("id"));
            version(dependency.at("version"));
            const auto origin = fields(dependency.at("origin"), {"kind", "author", "reference", "revision"});
            const auto kind = string(origin.at("kind"), "value");
            require(kind == "owned" || kind == "external", "value", origin.at("kind"));
            for(const auto key : {"author", "reference", "revision"}) metadata(origin.at(key));
            const auto license = fields(dependency.at("license"), {"identifier", "notice"});
            metadata(license.at("identifier"));
            const auto notice = identifier(license.at("notice"));
            const auto files = array(dependency.at("files"));
            require(result.requests.size() + files.size() <= resource_max_files, "budget", dependency.at("files"));
            std::set<std::string> file_ids;
            std::map<std::string, std::string> inventory;
            for(const auto file_token : files) {
                const auto file = fields(file_token, {"id", "path", "bytes", "sha256"});
                const auto fid = identifier(file.at("id"));
                require(file_ids.insert(fid).second, "id", file.at("id"));
                const auto path = portable_path(file.at("path"));
                require(paths.insert(lower(path)).second, "path", file.at("path"));
                const auto size = bytes(file.at("bytes"));
                require(size <= resource_max_total_bytes - result.declared_bytes, "budget", file.at("bytes"));
                result.declared_bytes += size;
                const auto sha = hash(file.at("sha256"));
                // Validated paths/hashes contain no JSON escape characters.
                inventory.emplace(path, "{\"bytes\":" + std::to_string(size) + ",\"path\":\"" + path + "\",\"sha256\":\"" + sha + "\"}");
                result.requests.push_back({id, fid, path, size, sha});
                result.source_offsets.push_back(syntax_->tokens[file_token].begin);
            }
            require(file_ids.contains(notice), "reference", license.at("notice"));
            const auto declared_hash = hash(dependency.at("content_sha256"));
            std::string canonical = "[";
            for(const auto& [path, row] : inventory) {
                (void)path;
                if(canonical.size() > 1) canonical += ',';
                canonical += row;
            }
            canonical += ']';
            require(detail::lock_sha256(canonical) == declared_hash, "hash", dependency.at("content_sha256"));
        }
        for(const auto& path : paths)
            for(auto end = path.find('/'); end != path.npos; end = path.find('/', end + 1))
                require(!paths.contains(path.substr(0, end)), "path", 0);
        return result;
    }
private:
    std::shared_ptr<const DeclarationSyntax> syntax_;
    void require(bool condition, const char* code, Index index) const
    {
        if(!condition) throw LockError{code, syntax_->tokens[index].begin};
    }
    Fields fields(Index index, std::initializer_list<const char*> names) const
    {
        const auto& token = syntax_->tokens[index];
        require(token.kind == JsonKind::object, "shape", index);
        Fields result;
        for(auto child = index + 1; child < token.next; child = syntax_->tokens[child + 1].next)
            result.emplace(syntax_->tokens[child].decoded, child + 1);
        require(result.size() == names.size(), "shape", index);
        for(const auto name : names) require(result.contains(name), "shape", index);
        return result;
    }
    std::vector<Index> array(Index index) const
    {
        const auto& token = syntax_->tokens[index];
        require(token.kind == JsonKind::array, "shape", index);
        std::vector<Index> result;
        for(auto child = index + 1; child < token.next; child = syntax_->tokens[child].next) result.push_back(child);
        require(!result.empty(), "shape", index);
        return result;
    }
    std::string string(Index index, const char* code) const
    {
        require(syntax_->tokens[index].kind == JsonKind::string, code, index);
        return syntax_->tokens[index].decoded;
    }
    std::string identifier(Index index) const
    {
        const auto value = string(index, "id");
        require(!value.empty() && value.size() <= 64 && value[0] >= 'a' && value[0] <= 'z'
            && std::all_of(value.begin(), value.end(), [](char c) { return (c >= 'a' && c <= 'z') || digit(c) || c == '_' || c == '-'; }), "id", index);
        return value;
    }
    void metadata(Index index) const
    {
        const auto value = string(index, "value");
        require(!value.empty() && value.size() <= 512 && value.front() != ' ' && value.back() != ' '
            && std::all_of(value.begin(), value.end(), [](char c) { return c >= 32 && c <= 126; }), "value", index);
    }
    void version(Index index) const
    {
        const auto value = string(index, "version");
        std::size_t start = 0;
        for(int part = 0; part < 3; ++part) {
            auto end = value.find('.', start);
            if(part == 2) { require(end == value.npos, "version", index); end = value.size(); }
            else require(end != value.npos, "version", index);
            require(end > start && end - start <= 6
                && std::all_of(value.begin() + static_cast<std::ptrdiff_t>(start), value.begin() + static_cast<std::ptrdiff_t>(end), digit), "version", index);
            start = end + 1;
        }
    }
    std::string hash(Index index) const
    {
        const auto value = string(index, "hash");
        require(value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) { return digit(c) || (c >= 'a' && c <= 'f'); }), "hash", index);
        return value;
    }
    std::uint64_t bytes(Index index) const
    {
        const auto& token = syntax_->tokens[index];
        require(token.kind == JsonKind::number, "budget", index);
        const auto raw = std::string_view(syntax_->bytes).substr(token.begin, token.end - token.begin);
        if(raw == "-0") return 0;
        std::uint64_t value = 0;
        const auto result = std::from_chars(raw.data(), raw.data() + raw.size(), value);
        require(result.ec == std::errc{} && result.ptr == raw.data() + raw.size() && value <= resource_max_file_bytes, "budget", index);
        return value;
    }
    std::string portable_path(Index index) const
    {
        const auto value = string(index, "path");
        require(!value.empty() && value.size() <= 240, "path", index);
        std::size_t start = 0, count = 0;
        while(start <= value.size()) {
            auto end = value.find('/', start);
            if(end == value.npos) end = value.size();
            const auto part = value.substr(start, end - start);
            const auto first = [](char c) { return alpha(c) || digit(c) || c == '_' || c == '-'; };
            require(++count <= 16 && !part.empty() && part.size() <= 80 && part.back() != '.'
                && first(part.front()) && std::all_of(part.begin(), part.end(), [&](char c) { return first(c) || c == '.'; }), "path", index);
            const auto stem = lower(part.substr(0, part.find('.')));
            require(stem != "con" && stem != "prn" && stem != "aux" && stem != "nul"
                && !(stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' && stem[3] <= '9'), "path", index);
            if(end == value.size()) break;
            start = end + 1;
        }
        return value;
    }
};
}
LockResult validate_lock_declaration(std::string_view bytes)
{
    try {
        const auto input = capture_declaration_syntax(bytes);
        if(const auto* error = std::get_if<IngressError>(&input))
            return LockError{error->code == IngressErrorCode::memory ? "memory" : "input", error->offset};
        return std::make_shared<const LockDeclaration>(Validator(std::get<0>(input)).run());
    } catch(const LockError& error) { return error; }
    catch(const std::bad_alloc&) { return LockError{"memory", 0}; }
}
}
