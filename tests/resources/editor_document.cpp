// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/editor_document.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* check)
{
    if(!value) throw std::runtime_error(check);
    std::cout << "PASS " << check << '\n';
}
bool opened(const simnodus::ProjectAcquisitionResult& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
bool revised(const simnodus::ProjectRevisionResult& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
bool saved(const simnodus::ProjectSaveResult& result)
{
    return std::holds_alternative<simnodus::ProjectSaveReceipt>(result);
}
}
int main(int argc, char** argv)
{
    try {
        simnodus::EditorDocument document;
        require(!document.graph() && !document.dirty() && document.bytes().empty(), "empty state");
        require(!revised(document.rename("name")) && !saved(document.save_copy("copy.json")), "no document refusal");
#ifndef _WIN32
        require(!opened(document.open("C:/not-a-native-root", "original.json")), "unsupported platform refusal");
        std::cout << "SKIP Windows local NTFS lifecycle\n";
        return 0;
#else
        if(argc != 2) return 2;
        const std::string root = argv[1];
        require(opened(document.open(root, "original.json")), "inert open without resources");
        const auto original = document.graph();
        const auto original_bytes = std::string(document.bytes());
        const auto original_name = std::string(document.name());
        require(!document.dirty() && !original->declaration->runtime_profile_verified &&
            !original->declaration->firmware_verified && !original->declaration->sources.resource_interfaces_verified,
            "clean unverified declaration");
        require(revised(document.rename(original_name)) && !document.dirty(), "same name clean");
        require(revised(document.rename("Edited \"RC\" \\ \xCE\xA9")) && document.dirty(), "Unicode escaped name edit");
        const auto revision = document.graph();
        const auto revision_bytes = std::string(document.bytes());
        require(original->declaration->project_id == revision->declaration->project_id &&
            original->connectivity.root == revision->connectivity.root && original->source_map.size() == revision->source_map.size(),
            "stable project and source identities");
        auto left = original->source_map.begin(); auto right = revision->source_map.begin();
        bool identities_match = true;
        for(; left != original->source_map.end(); ++left, ++right) identities_match = identities_match && left->first == right->first;
        require(identities_match, "stable graph source identities");
        require(!revised(document.rename("")) && !revised(document.rename(std::string(81, 'x'))) &&
            !revised(document.rename(std::string("bad\nname"))) && document.graph() == revision && document.dirty(),
            "invalid names preserve revision");
        require(!opened(document.open(root, "invalid.json")) && !opened(document.open(root, "missing.json")) &&
            document.graph() == revision && document.root() == root && document.leaf() == "original.json" && document.dirty(),
            "failed opens preserve association and edit");
        require(!saved(document.save_copy("occupied.json")) && !saved(document.save_copy("original.json")) &&
            !saved(document.save_copy("ORIGINAL.JSON")) && !saved(document.save_copy("../escape.json")) &&
            document.graph() == revision && document.dirty(), "save refusals preserve edit");
        require(saved(document.save_copy("copy.json")) && document.graph() == revision && document.dirty() &&
            document.leaf() == "original.json", "explicit copy retains dirty association");
        simnodus::EditorDocument reopened;
        require(opened(reopened.open(root, "copy.json")) && reopened.bytes() == revision_bytes && !reopened.dirty(), "exact copy reopen");
        require(revised(document.rename(original_name)) && document.bytes() == original_bytes && !document.dirty(), "return to original is clean");
        require(revised(document.rename("unsaved")), "prepare missing directory refusal");
        const auto before = document.graph();
        std::filesystem::rename(root, root + "-moved");
        require(!saved(document.save_copy("new.json")) && !saved(document.save_copy("original.json")) &&
            document.graph() == before && document.dirty(), "missing root preserves unsaved edit");
        std::filesystem::rename(root + "-moved", root);
        std::filesystem::rename(std::filesystem::path(root) / "original.json", std::filesystem::path(root) / "moved-original.json");
        require(!saved(document.save_copy("original.json")) && !saved(document.save_copy("ORIGINAL.JSON")), "original name refused even when absent");
        std::filesystem::rename(std::filesystem::path(root) / "moved-original.json", std::filesystem::path(root) / "original.json");
        return 0;
#endif
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
