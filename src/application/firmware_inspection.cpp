// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/firmware_inspection.hpp"
#include <map>
#include <new>

namespace simnodus {
FirmwareInspectionResult inspect_project_firmware(std::string_view bytes,
    const std::string& root, std::string_view id)
{
    auto stage = FirmwareInspectionStage::declaration;
    try {
        const auto validated = validate_project_declaration(bytes);
        if(const auto* e = std::get_if<LockError>(&validated))
            return FirmwareInspectionError{stage, e->code, e->offset};
        stage = FirmwareInspectionStage::selection;
        if(id.empty() || id.size() > 64) return FirmwareInspectionError{stage, "firmware"};
        auto result = std::make_shared<FirmwareInspection>();
        result->declaration = std::get<0>(validated);
        const auto& syntax = *result->declaration->sources.lock.syntax;
        const auto object = [&](std::size_t index) {
            std::map<std::string, std::size_t> fields;
            for(auto k = index + 1; k < syntax.tokens[index].next; k = syntax.tokens[k + 1].next)
                fields.emplace(syntax.tokens[k].decoded, k + 1);
            return fields;
        };
        const auto array = object(0).at("firmware");
        std::size_t selected = 0;
        for(auto k = array + 1; k < syntax.tokens[array].next; k = syntax.tokens[k].next)
            if(syntax.tokens[object(k).at("id")].decoded == id) { selected = k; break; }
        if(selected == 0) return FirmwareInspectionError{stage, "firmware", syntax.tokens[array].begin};
        const auto fields = object(selected), reference = object(fields.at("resource"));
        const auto& dependency = syntax.tokens[reference.at("dependency")].decoded;
        const auto& resource = syntax.tokens[reference.at("resource")].decoded;
        result->firmware_id = id;
        result->declared_architecture = syntax.tokens[fields.at("architecture")].decoded;
        result->firmware_offset = syntax.tokens[selected].begin;
        result->architecture_offset = syntax.tokens[fields.at("architecture")].begin;
        stage = FirmwareInspectionStage::resources;
        const auto captured = verify_local_resources(root, result->declaration->sources.lock.requests);
        if(const auto* e = std::get_if<ResourceError>(&captured))
            return FirmwareInspectionError{stage, resource_error_name(e->code), e->index, e->system_code, "resource-index"};
        result->resources = std::get<0>(captured);
        std::size_t index = 0;
        for(; index < result->resources->size(); ++index) {
            const auto& row = (*result->resources)[index];
            if(row.dependency == dependency && row.resource == resource) break;
        }
        // Whole declaration validation guarantees this reference is in the lock.
        if(index == result->resources->size()) return FirmwareInspectionError{stage, "resource", index, 0, "resource-index"};
        result->resource_index = index;
        stage = FirmwareInspectionStage::elf;
        const auto& data = (*result->resources)[index].data;
        const auto view = data.empty() ? std::string_view{} :
            std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
        const auto inspected = firmware::inspect_elf32(view);
        if(const auto* e = std::get_if<firmware::ElfInspectionError>(&inspected))
            return FirmwareInspectionError{stage, e->code, e->offset, 0, "elf-bytes"};
        result->elf = std::get<0>(inspected);
        return std::shared_ptr<const FirmwareInspection>(std::move(result));
    } catch(const std::bad_alloc&) { return FirmwareInspectionError{stage, "memory"}; }
}
}
