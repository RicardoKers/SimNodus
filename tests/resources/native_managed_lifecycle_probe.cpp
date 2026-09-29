// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Manual disposable-VM composition probe. No engine is linked or launched.
#include "application/ideal_rc_compilation.hpp"
#include "application/project_revision.hpp"
#include "application/project_save.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <io.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace simnodus;
constexpr std::size_t maximum = declaration_max_bytes + 16 * 1024 + 256;
void need(bool condition, const char* code)
{
    if(!condition) throw std::runtime_error(code);
}
std::string read(const char* path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    need(file.good(), "input-open");
    const auto length = file.tellg();
    need(length >= 0 && static_cast<std::uint64_t>(length) <= maximum, "input-bound");
    file.seekg(0);
    std::string result(static_cast<std::size_t>(length), '\0');
    file.read(result.data(), static_cast<std::streamsize>(result.size()));
    need(file.good(), "input-read");
    return result;
}
struct Reader {
    std::string_view bytes;
    std::size_t offset{};
    std::string_view take(std::size_t length)
    {
        need(length <= bytes.size() - offset, "frame-bound");
        const auto result = bytes.substr(offset, length);
        offset += length;
        return result;
    }
    std::uint64_t integer(unsigned width)
    {
        const auto data = take(width);
        std::uint64_t value = 0;
        for(unsigned i = 0; i < width; ++i)
            value |= static_cast<std::uint64_t>(static_cast<unsigned char>(data[i])) << (8 * i);
        return value;
    }
    template<std::size_t N> std::array<unsigned char, N> array()
    {
        std::array<unsigned char, N> value{};
        const auto data = take(N);
        std::copy(data.begin(), data.end(), value.begin());
        return value;
    }
    bool done() const noexcept { return offset == bytes.size(); }
};
struct Opened {
    std::string project, locator;
    PhysicalRootIdentity root;
    std::uint32_t revision{};
};
Opened open_response(std::string_view request_bytes, std::string_view response_bytes)
{
    Reader request{request_bytes};
    need(request.take(8) == "SN21SAVE" && request.integer(2) == 3
        && request.integer(2) == 1 && request.integer(4) == request_bytes.size() - 32,
        "request-header");
    const auto correlation = request.array<16>();
    request.take(16); // Run ID is validated by the authenticated request endpoint.
    const auto generation = request.array<16>(), document = request.array<16>();
    need(request.done(), "request-trailing");

    Reader response{response_bytes};
    need(response.take(8) == "SN21SAVE" && response.integer(2) == 3
        && response.integer(2) == 0x8001 && response.integer(4) == response_bytes.size() - 32
        && response.array<16>() == correlation && response.integer(4) == 0,
        "response-header");
    need(response.array<16>() == generation && response.array<16>() == document,
        "response-scope");
    Opened result;
    result.revision = static_cast<std::uint32_t>(response.integer(4));
    need(result.revision >= 1 && result.revision <= 64, "response-revision");
    response.take(16 + 32 + 8 + 16); // Remaining token fields are checked by the request client.
    const auto context_length = response.integer(4);
    need(context_length >= 52 && context_length <= 16 * 1024, "context-bound");
    Reader context{response.take(static_cast<std::size_t>(context_length))};
    need(context.take(4) == "SRC1", "context-format");
    context.take(16); // Binding ID is retained in the authenticated store record.
    result.root.volume = context.integer(8);
    result.root.file = context.array<16>();
    need(context.integer(4) == 1, "context-policy");
    const auto locator_length = context.integer(4);
    need(locator_length > 0 && locator_length <= 4096, "context-locator");
    result.locator = context.take(static_cast<std::size_t>(locator_length));
    need(context.done(), "context-trailing");
    const auto project_length = response.integer(4);
    need(project_length > 0 && project_length <= declaration_max_bytes, "project-bound");
    result.project = response.take(static_cast<std::size_t>(project_length));
    need(response.done(), "response-trailing");
    return result;
}
std::wstring wide(std::string_view value)
{
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    need(count > 0 && count <= 32767, "output-path");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    need(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), count) == count, "output-path");
    return result;
}
void create_only(std::string_view path, std::string_view bytes)
{
    const auto location = wide(path);
    HANDLE file = CreateFileW(location.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    need(file != INVALID_HANDLE_VALUE, "output-create");
    DWORD written = 0;
    const bool success = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
        &written, nullptr) && written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    need(success, "output-write");
}
}

int main(int argc, char** argv)
{
    if(argc >= 3 && std::string_view(argv[1]) == "--report") {
        const auto redirect = [](const std::string& path, FILE* stream) {
            HANDLE raw = CreateFileW(wide(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if(raw == INVALID_HANDLE_VALUE) return false;
            const int descriptor = _open_osfhandle(reinterpret_cast<intptr_t>(raw), _O_WRONLY | _O_BINARY);
            if(descriptor < 0) { CloseHandle(raw); return false; }
            const bool copied = _dup2(descriptor, _fileno(stream)) == 0;
            _close(descriptor);
            return copied;
        };
        if(!redirect(argv[2], stdout) || !redirect(std::string(argv[2]) + ".stderr", stderr)) return 3;
        argc -= 2;
        argv += 2;
    }
    std::cout << std::unitbuf;
    try {
        need(argc >= 5, "arguments");
        const std::string_view mode(argv[1]);
        const auto opened = open_response(read(argv[2]), read(argv[3]));
        if(mode == "edit") {
            need(argc == 6, "arguments");
            const auto renamed = rename_project(opened.project, argv[5]);
            if(const auto* error = std::get_if<ProjectRevisionError>(&renamed))
                throw std::runtime_error(std::string("edit-") + error->code);
            const auto& bytes = std::get<0>(renamed)->declaration->sources.lock.syntax->bytes;
            create_only(argv[4], bytes);
            std::cout << "{\"status\":\"edited\",\"revision\":" << opened.revision
                << ",\"bytes\":" << bytes.size() << "}\n";
        } else if(mode == "export") {
            need(argc == 6, "arguments");
            const auto saved = save_new_project(argv[4], argv[5], opened.project);
            if(const auto* error = std::get_if<ProjectSaveError>(&saved))
                throw std::runtime_error(std::string("export-") + error->code);
            std::cout << "{\"status\":\"exported\",\"revision\":" << opened.revision
                << ",\"bytes\":" << opened.project.size() << "}\n";
        } else if(mode == "compile") {
            need(argc == 5, "arguments");
            const IdealRcRequest selection{IdealRcProfile::e01_ideal_rc,
                "reference", "source", "left_out"};
            const auto result = compile_fixed_rc_replay_bound(
                opened.project, opened.locator, selection, opened.root);
            if(const auto* error = std::get_if<IdealRcError>(&result))
                throw std::runtime_error(std::string("compile-") + error->code);
            const auto& compiled = *std::get<0>(result);
            need(compiled.replay.has_value() &&
                !compiled.connectivity->source->declaration->runtime_profile_verified &&
                !compiled.connectivity->source->declaration->sources.topology.simulation_ready,
                "readiness");
            create_only(argv[4], compiled.netlist);
            std::cout << "{\"status\":\"compiled-bound\",\"revision\":" << opened.revision
                << ",\"netlist_bytes\":" << compiled.netlist.size()
                << ",\"schedule_resource\":" << compiled.replay->schedule_resource_index
                << ",\"duration_ns\":" << compiled.replay->duration_ns
                << ",\"exchange_quantum_ns\":" << compiled.replay->exchange_quantum_ns
                << ",\"readiness\":false}\n";
        } else need(false, "mode");
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::cout << "{\"status\":\"fixture-failed\"}\n";
        return 2;
    }
}
