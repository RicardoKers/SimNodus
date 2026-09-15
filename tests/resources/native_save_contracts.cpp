// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_save.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv)
{
    using namespace simnodus;
    if(argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) return 1;
    const std::string valid(std::istreambuf_iterator<char>{input}, {});
    unsigned checks = 0;
    const auto invalid = [&](const std::string& root, const std::string& name, std::string_view raw, const char* code) {
        ++checks;
        const auto result = save_new_project(root, name, raw);
        const auto* error = std::get_if<ProjectSaveError>(&result);
        return error && std::string_view(error->code) == code && !error->temporary_cleanup_failed;
    };
    for(const auto name : {"", "../project.json", "dir/project.json", "C:/project.json", "project:stream", "CON", "aux.json", "COM1.txt", "a.", "a ", "a~1", "a\\b"})
        if(!invalid("C:/missing-save-contract-root", name, valid, "path")) return 1;
    if(!invalid("C:/missing-save-contract-root", std::string(81, 'a'), valid, "path")) return 1;
    for(const auto root : {".", "C:relative", "//server/share", "\\\\?\\C:\\root", "C:/../root", "C:/a//b", "C:/a~1", "C:/a.", "C:/a ", "C:/NUL"})
        if(!invalid(root, "project.json", valid, "root")) return 1;
    if(!invalid(".", "../bad", "{}", "shape")) return 1;
    if(!invalid(".", "../bad", std::string(1024 * 1024 + 1, ' '), "input")) return 1;
#ifndef _WIN32
    if(!invalid("C:/root", "project.json", valid, "platform")) return 1;
#endif
    std::cout << checks << " create-only persistence boundary checks passed\n";
}
