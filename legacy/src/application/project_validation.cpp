// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_validation.hpp"
#include "application/captured_validation_internal.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <new>
#include <set>

namespace simnodus {
namespace {
using Index = std::size_t;
using Fields = std::map<std::string, Index>;
class Validator {
public:
    explicit Validator(std::shared_ptr<const DeclarationSyntax> source) : source_(std::move(source)) {}
    ProjectDeclaration run()
    {
        const auto root = exact(0, {"format", "version", "id", "name", "sources", "platforms", "firmware", "targets", "temporal"});
        require(is_text(root.at("format"), "simnodus-project") && is_text(root.at("version"), "0.1"), "version", 0);
        const auto project_id = named(root);
        auto sources = detail::captured_links(source_, root.at("sources"));
        for(const auto& row : sources.lock.requests) files_.emplace(row.dependency, row.resource);
        const auto platforms = records(root.at("platforms"), {"id", "name", "mcu", "board", "pins"});
        for(const auto& [id, token] : platforms) {
            (void)id;
            const auto platform = object(token);
            const auto mcu = exact(platform.at("mcu"), {"device", "architecture", "resource"});
            identifier(mcu.at("device"));
            identifier(mcu.at("architecture"));
            reference(mcu.at("resource"));
            if(!is_null(platform.at("board"))) {
                const auto board = exact(platform.at("board"), {"id", "name", "resource"});
                named(board);
                reference(board.at("resource"));
                ++count_;
            }
            records(platform.at("pins"), {"id", "name"});
        }
        const auto firmware = records(root.at("firmware"), {"id", "name", "architecture", "image_format", "resource"});
        for(const auto& [id, token] : firmware) {
            (void)id;
            const auto image = object(token);
            identifier(image.at("architecture"));
            require(is_text(image.at("image_format"), "elf"), "kind", image.at("image_format"));
            reference(image.at("resource"));
        }
        const auto topology = object(object(root.at("sources")).at("topology"));
        Fields components;
        for(const auto token : array(topology.at("components"))) components.emplace(identifier(object(token).at("id")), token);
        std::map<std::vector<std::string>, std::string> paths;
        for(const auto& row : sources.topology.parameters.instances) paths.emplace(row.path, row.definition);
        std::set<std::vector<std::string>> seen;
        const auto targets = array(root.at("targets"));
        require(targets.size() <= 256, "budget", root.at("targets"));
        for(const auto token : targets) {
            const auto target = exact(token, {"path", "platform", "firmware", "pin_map"});
            const auto parts = array(target.at("path"));
            require(parts.size() >= 2, "shape", target.at("path"));
            require(parts.size() <= 9, "budget", target.at("path"));
            std::vector<std::string> path;
            for(const auto part : parts) path.push_back(identifier(part));
            const auto occurrence = paths.find(path);
            require(occurrence != paths.end() && components.contains(occurrence->second), "reference", target.at("path"));
            require(seen.insert(path).second, "reference", target.at("path"));
            const auto pid = identifier(target.at("platform")), fid = identifier(target.at("firmware"));
            require(platforms.contains(pid) && firmware.contains(fid), "reference", token);
            const auto platform = object(platforms.at(pid)), image = object(firmware.at(fid));
            require(source_->tokens[object(platform.at("mcu")).at("architecture")].decoded == source_->tokens[image.at("architecture")].decoded, "mapping", token);
            const auto component = object(components.at(occurrence->second));
            require(is_null(component.at("model")), "mapping", token);
            const auto mapping = object(target.at("pin_map"), "mapping");
            const auto pins = pin_ids(component.at("pins"));
            require(mapping.size() == pins.size(), "mapping", target.at("pin_map"));
            std::set<std::string> destinations;
            for(const auto& [pin, destination] : mapping) {
                require(pins.contains(pin), "mapping", target.at("pin_map"));
                require(destinations.insert(identifier(destination)).second, "mapping", destination);
            }
            require(destinations == pin_ids(platform.at("pins")), "mapping", target.at("pin_map"));
            count_ += 1 + mapping.size();
        }
        require(count_ <= 256, "budget", 0);
        temporal(root.at("temporal"));
        return {std::move(sources), project_id, targets.size(), count_, false, false};
    }
private:
    std::shared_ptr<const DeclarationSyntax> source_;
    std::set<std::pair<std::string, std::string>> files_;
    std::size_t count_ = 0;
    void require(bool value, const char* code, Index index) const
    {
        if(!value) throw LockError{code, source_->tokens[index].begin};
    }
    Fields object(Index index, const char* code = "shape") const
    {
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::object, code, index);
        Fields result;
        for(auto child = index + 1; child < token.next; child = source_->tokens[child + 1].next)
            result.emplace(source_->tokens[child].decoded, child + 1);
        return result;
    }
    Fields exact(Index index, std::initializer_list<const char*> names) const
    {
        auto result = object(index);
        require(result.size() == names.size(), "shape", index);
        for(const auto name : names) require(result.contains(name), "shape", index);
        return result;
    }
    std::vector<Index> array(Index index) const
    {
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::array, "shape", index);
        std::vector<Index> result;
        for(auto child = index + 1; child < token.next; child = source_->tokens[child].next) result.push_back(child);
        return result;
    }
    bool is_null(Index index) const { return source_->tokens[index].kind == JsonKind::null; }
    bool is_text(Index index, std::string_view expected) const
    {
        const auto& token = source_->tokens[index];
        return token.kind == JsonKind::string && token.decoded == expected;
    }
    std::string identifier(Index index) const
    {
        const auto& token = source_->tokens[index];
        const auto& value = token.decoded;
        require(token.kind == JsonKind::string && !value.empty() && value.size() <= 64 && value[0] >= 'a' && value[0] <= 'z'
            && std::all_of(value.begin(), value.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; }), "id", index);
        return value;
    }
    std::string named(const Fields& fields) const
    {
        const auto id = identifier(fields.at("id"));
        const auto index = fields.at("name");
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::string, "value", index);
        std::size_t length = 0;
        unsigned int scalar = 0;
        // Native ingress already checked UTF-8 and rejected surrogate scalars.
        for(const auto byte : token.decoded) {
            const auto c = static_cast<unsigned char>(byte);
            if((c & 0xc0) != 0x80) {
                if(length) require(scalar >= 32 && !(scalar >= 127 && scalar <= 159), "value", index);
                ++length;
                scalar = c < 128 ? c : c < 224 ? c & 31 : c < 240 ? c & 15 : c & 7;
            } else scalar = (scalar << 6) | (c & 63);
        }
        require(length > 0 && length <= 80 && scalar >= 32 && !(scalar >= 127 && scalar <= 159), "value", index);
        return id;
    }
    Fields records(Index index, std::initializer_list<const char*> names)
    {
        Fields result;
        for(const auto token : array(index)) {
            const auto record = exact(token, names);
            require(result.emplace(named(record), token).second, "id", token);
            require(++count_ <= 256, "budget", token);
        }
        return result;
    }
    void reference(Index index) const
    {
        const auto value = exact(index, {"dependency", "resource"});
        const auto dependency = identifier(value.at("dependency")), resource = identifier(value.at("resource"));
        require(files_.contains({dependency, resource}), "reference", index);
    }
    std::set<std::string> pin_ids(Index index) const
    {
        std::set<std::string> result;
        for(const auto token : array(index)) result.insert(identifier(object(token).at("id")));
        return result;
    }
    std::uint64_t integer(Index index) const
    {
        const auto& token = source_->tokens[index];
        require(token.kind == JsonKind::number, "range", index);
        const auto raw = std::string_view(source_->bytes).substr(token.begin, token.end - token.begin);
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), value);
        require(parsed.ec == std::errc{} && parsed.ptr == raw.data() + raw.size(), "range", index);
        return value;
    }
    void temporal(Index index) const
    {
        const auto policy = exact(index, {"mode", "fidelity", "duration_ns", "exchange_quantum_ns", "schedule", "debug", "pause_wall_timeout_ms", "voltage_tolerance_uv", "analog_time_tolerance_ps", "on_unsupported"});
        const bool unconfigured = is_text(policy.at("mode"), "unconfigured");
        const bool replay = is_text(policy.at("mode"), "known-schedule-replay");
        require(unconfigured || replay || is_text(policy.at("mode"), "approximate-sampled"), "capability", policy.at("mode"));
        require(is_text(policy.at("on_unsupported"), "reject"), "capability", policy.at("on_unsupported"));
        for(const auto& [key, expected] : std::initializer_list<std::pair<const char*, std::uint64_t>>{{"pause_wall_timeout_ms", 2000}, {"voltage_tolerance_uv", 10}, {"analog_time_tolerance_ps", 1}})
            require(integer(policy.at(key)) == expected, "range", policy.at(key));
        if(unconfigured) {
            for(const auto key : {"fidelity", "duration_ns", "exchange_quantum_ns", "schedule"}) require(is_null(policy.at(key)), "shape", policy.at(key));
            require(is_text(policy.at("debug"), "disabled"), "shape", policy.at("debug"));
        } else {
            require(is_text(policy.at("fidelity"), replay ? "causal-replay" : "approximate"), "capability", policy.at("fidelity"));
            const auto duration = integer(policy.at("duration_ns")), quantum = integer(policy.at("exchange_quantum_ns"));
            require(duration > 0 && duration % 1000 == 0, "range", policy.at("duration_ns"));
            require(quantum > 0 && quantum % 1000 == 0 && quantum <= duration, "range", policy.at("exchange_quantum_ns"));
            if(replay) reference(policy.at("schedule"));
            else require(is_null(policy.at("schedule")), "shape", policy.at("schedule"));
            require(is_text(policy.at("debug"), "disabled") || is_text(policy.at("debug"), "bounded-cooperative"), "capability", policy.at("debug"));
        }
    }
};
}
ProjectResult validate_project_declaration(std::string_view bytes)
{
    try {
        const auto input = capture_declaration_syntax(bytes);
        if(const auto* error = std::get_if<IngressError>(&input))
            return LockError{error->code == IngressErrorCode::memory ? "memory" : "input", error->offset};
        return std::make_shared<const ProjectDeclaration>(Validator(std::get<0>(input)).run());
    } catch(const TopologyError& error) { return LockError{topology_error_name(error.code), error.offset}; }
    catch(const ParameterError& error) { return LockError{error.code, error.offset}; }
    catch(const LockError& error) { return error; }
    catch(const std::bad_alloc&) { return LockError{"memory", 0}; }
}
}
