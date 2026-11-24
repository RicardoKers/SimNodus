// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The bundled example projects (SN-123) must open and pass the pre-run
// checks without errors or warnings.
#include "test_support.hpp"

#include "io/project_file.hpp"
#include "library/blocks.hpp"
#include "netlist/netlist.hpp"

#include <cctype>
#include <filesystem>

using namespace simnodus;

TEST_CASE("example projects open and pass the pre-run checks")
{
    library::ComponentLibrary library;
    library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    int count = 0;
    // Examples are in category folders ("01_Analog_circuits"); blocks/ holds block files.
    for (const auto& entry : std::filesystem::recursive_directory_iterator(SIMNODUS_TEST_EXAMPLES_DIR)) {
        const auto folder = entry.path().parent_path().filename().string();
        if (entry.path().extension() != ".snx" || folder.empty() || !std::isdigit(static_cast<unsigned char>(folder[0]))) {
            continue;
        }
        ++count;
        const auto loaded = io::load_project(entry.path());
        for (const auto& e : loaded.errors) {
            std::printf("  %s: %s\n", entry.path().filename().string().c_str(), e.c_str());
        }
        REQUIRE(loaded.schematic.has_value());
        CHECK(loaded.warnings.empty());
        // A project's blocks (PRJ-06) come with it.
        const auto n = netlist::generate_netlist(*loaded.schematic, library::with_blocks(library, *loaded.schematic));
        for (const auto& d : n.diagnostics) {
            std::printf("  %s: [%s] %s\n", entry.path().filename().string().c_str(), d.code.c_str(), d.message.c_str());
        }
        CHECK(n.diagnostics.empty());
        CHECK(!loaded.schematic->probes().empty());
    }
    CHECK(count >= 6);
}
