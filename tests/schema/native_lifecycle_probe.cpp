// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Test-only orchestration. No engine is linked or launched.
#include "application/project_acquisition.hpp"
#include "application/project_revision.hpp"
#include "application/project_save.hpp"
#include "application/ideal_rc_compilation.hpp"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
std::string field()
{
    unsigned size = 0;
    for(unsigned i = 0; i < 4; ++i) {
        const auto byte = std::cin.get();
        if(byte < 0) throw std::runtime_error("truncated");
        size |= static_cast<unsigned>(byte) << (8 * i);
    }
    if(size > simnodus::declaration_max_bytes) throw std::runtime_error("limit");
    std::string value(size, '\0');
    std::cin.read(value.data(), size);
    if(!std::cin) throw std::runtime_error("truncated");
    return value;
}
int failure(const char* stage, const char* code)
{
    std::cout << "ERR " << stage << ' ' << code << '\n';
    return 1;
}
void hex(const std::string& value)
{
    for(unsigned char c : value)
        std::cout << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
    std::cout << '\n';
}
const std::string& bytes(const simnodus::ProjectGraph& graph)
{
    return graph.declaration->sources.lock.syntax->bytes;
}
}
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    try {
        const auto root = field();
        auto original = field();
        const auto name = field();
        const auto saved = simnodus::save_new_project(root, "original.json", original);
        if(const auto* e = std::get_if<simnodus::ProjectSaveError>(&saved)) return failure("create", e->code);
        const auto opened = simnodus::acquire_project(root, "original.json");
        if(const auto* e = std::get_if<simnodus::ProjectAcquisitionError>(&opened)) return failure("open", e->code);
        const auto captured = std::get<0>(opened);
        if(bytes(*captured) != original) return failure("open", "bytes");
        original.clear();
        // The Python test may replace fixture-owned files after capture.
        // This barrier is test transport, never a production ownership protocol.
        std::cout << "CAPTURED\n" << std::flush;
        if(std::cin.get() != 'c') return failure("transport", "resume");
        const auto edited = simnodus::rename_project(bytes(*captured), name);
        if(const auto* e = std::get_if<simnodus::ProjectRevisionError>(&edited)) return failure("rename", e->code);
        const auto revised = std::get<0>(edited);
        const auto copied = simnodus::save_new_project(root, "revised.json", bytes(*revised));
        if(const auto* e = std::get_if<simnodus::ProjectSaveError>(&copied)) return failure("save-copy", e->code);
        const auto reopened = simnodus::acquire_project(root, "revised.json");
        if(const auto* e = std::get_if<simnodus::ProjectAcquisitionError>(&reopened)) return failure("reopen", e->code);
        const auto loaded = std::get<0>(reopened);
        if(bytes(*loaded) != bytes(*revised)) return failure("reopen", "bytes");
        const auto result = simnodus::compile_ideal_rc(bytes(*loaded), root,
            {simnodus::IdealRcProfile::e01_ideal_rc, "reference", "source", "left_out"});
        if(const auto* e = std::get_if<simnodus::IdealRcError>(&result)) return failure("compile", e->code);
        const auto compiled = std::get<0>(result);
        if(compiled->connectivity->source->declaration->runtime_profile_verified ||
            compiled->connectivity->source->declaration->sources.topology.simulation_ready)
            return failure("compile", "readiness");
        std::cout << "OK\n";
        hex(bytes(*captured));hex(bytes(*loaded));hex(compiled->netlist);
        for(const auto& e : compiled->elements) {
            std::cout << e.project_parameter_offset << ' ' << e.project_instance.begin << ' ' << e.project_instance.end << '\n';
        }
    } catch(const std::exception&) { return failure("transport", "exception"); }
}
