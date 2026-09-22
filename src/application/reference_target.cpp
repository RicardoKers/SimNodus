// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/reference_target.hpp"
#include <new>
#include <set>

namespace simnodus {
ReferenceTargetResult inspect_reference_target(std::string_view bytes, const std::string& root,
    const std::vector<std::string>& path, firmware::BootProfile profile)
{
    try {
        if(profile != firmware::BootProfile::sn012_stm32f103c8) return ReferenceTargetError{"request", "profile"};
        if(path.size() < 2 || path.size() > 9) return ReferenceTargetError{"request", "path"};
        for(const auto& part : path) if(part.empty() || part.size() > 64) return ReferenceTargetError{"request", "path"};
        const auto validated = validate_project_declaration(bytes);
        if(const auto* e = std::get_if<LockError>(&validated)) return ReferenceTargetError{"declaration", e->code, e->offset};
        const auto& syntax = *std::get<0>(validated)->sources.lock.syntax;
        const auto object = [&](std::size_t at) {
            std::map<std::string, std::size_t> fields;
            for(auto k = at + 1; k < syntax.tokens[at].next; k = syntax.tokens[k + 1].next)
                fields.emplace(syntax.tokens[k].decoded, k + 1);
            return fields;
        };
        const auto array = [&](std::size_t at) {
            std::vector<std::size_t> values;
            for(auto k = at + 1; k < syntax.tokens[at].next; k = syntax.tokens[k].next) values.push_back(k);
            return values;
        };
        const auto text = [&](std::size_t at) -> const std::string& { return syntax.tokens[at].decoded; };
        const auto require = [&](bool ok, const char* code, std::size_t token) {
            if(!ok) throw ReferenceTargetError{"selection", code, syntax.tokens[token].begin};
        };
        const auto document = object(0), temporal = object(document.at("temporal"));
        require(text(temporal.at("mode")) == "unconfigured", "standalone", temporal.at("mode"));
        const auto targets = array(document.at("targets"));
        require(targets.size() == 1, "target-count", document.at("targets"));
        const auto target = object(targets.front());
        std::vector<std::string> declared_path;
        for(const auto k : array(target.at("path"))) declared_path.push_back(text(k));
        require(path == declared_path, "target", target.at("path"));
        const auto platform_id = text(target.at("platform"));
        std::size_t selected = 0;
        for(const auto k : array(document.at("platforms")))
            if(text(object(k).at("id")) == platform_id) { selected = k; break; }
        require(selected != 0, "platform", target.at("platform"));
        const auto platform = object(selected), mcu = object(platform.at("mcu"));
        require(text(mcu.at("device")) == "stm32f103c8", "device", mcu.at("device"));
        require(text(mcu.at("architecture")) == "arm-cortex-m3", "architecture", mcu.at("architecture"));
        require(syntax.tokens[platform.at("board")].kind == JsonKind::null, "offline-board", platform.at("board"));
        std::set<std::string> pins;
        for(const auto k : array(platform.at("pins"))) pins.insert(text(object(k).at("id")));
        require(pins == std::set<std::string>{"pa0", "pa1", "pa2", "pa3", "pa4"}, "pins", platform.at("pins"));
        const auto inspected = inspect_project_firmware(syntax.bytes, root, text(target.at("firmware")));
        if(const auto* e = std::get_if<FirmwareInspectionError>(&inspected))
            return ReferenceTargetError{"firmware", e->code, e->offset, e->system, e->coordinate};
        auto result = std::make_shared<ReferenceTarget>();
        result->firmware = std::get<0>(inspected);
        const auto reference = object(mcu.at("resource"));
        const auto& inventory = *result->firmware->resources;
        std::size_t index = 0;
        for(; index < inventory.size(); ++index)
            if(inventory[index].dependency == text(reference.at("dependency")) && inventory[index].resource == text(reference.at("resource"))) break;
        require(index < inventory.size(), "platform-resource", mcu.at("resource"));
        // These pins identify the owned reference experiment, not user trust policy.
        require(inventory[index].sha256 == "e8c8e3b588a80573cf98fabdebc78afa499f8504a10bf01fb89c7ac47016837b", "platform-bytes", mcu.at("resource"));
        require(inventory[result->firmware->resource_index].sha256 == "7895196a7e63134e5576f9bae2f4b124e7b0f1a3487891bea5a6ad56576b3689", "firmware-bytes", target.at("firmware"));
        const auto boot = firmware::inspect_boot_candidate(result->firmware->elf->bytes,
            result->firmware->declared_architecture, profile);
        if(const auto* e = std::get_if<firmware::BootCandidateError>(&boot))
            return ReferenceTargetError{"boot", e->code, e->offset, 0, "elf-bytes"};
        result->boot = std::get<0>(boot);result->path = path;result->platform_id = platform_id;
        result->target_offset = syntax.tokens[targets.front()].begin;
        result->platform_offset = syntax.tokens[selected].begin;result->platform_resource_index = index;
        for(const auto& [pin, token] : object(target.at("pin_map"))) result->pin_map.emplace(pin, text(token));
        return std::shared_ptr<const ReferenceTarget>(std::move(result));
    } catch(const ReferenceTargetError& e) { return e; }
    catch(const std::bad_alloc&) { return ReferenceTargetError{"operation", "memory"}; }
}
}
