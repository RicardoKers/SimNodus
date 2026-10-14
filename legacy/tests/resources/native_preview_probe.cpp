// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/symbol_preview.hpp"
#include "application/editor_document.hpp"
#include "../schema/graph_probe_output.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
void geometry(const simnodus::symbols::FixtureArtwork& artwork)
{
    std::cout << "{\"width\":" << artwork.width << ",\"height\":" << artwork.height << ",\"lines\":";
    graph_probe::array(artwork.lines, [](const auto& line) {
        std::cout << '[' << line.x1 << ',' << line.y1 << ',' << line.x2 << ',' << line.y2 << ']';
    });
    std::cout << '}';
}
int error(const simnodus::SymbolPreviewError& value)
{
    std::cout << "{\"error\":\"" << value.code << "\",\"stage\":" << static_cast<int>(value.stage) << "}\n";
    return 1;
}
void selection(const simnodus::SymbolPreviewSelection& value)
{
    std::cout << "{\"component\":"; graph_probe::quote(value.component);
    std::cout << ",\"symbol\":"; graph_probe::quote(value.symbol);
    std::cout << ",\"asset\":"; graph_probe::quote(value.asset);
    const auto& r = value.request;
    std::cout << ",\"request\":{\"dependency\":"; graph_probe::quote(r.dependency);
    std::cout << ",\"resource\":"; graph_probe::quote(r.resource);
    std::cout << ",\"path\":"; graph_probe::quote(r.path);
    std::cout << ",\"sha256\":"; graph_probe::quote(r.sha256);
    std::cout << ",\"bytes\":" << r.bytes << "}}";
}
void require(bool value, const char* check)
{
    if(!value) throw std::runtime_error(check);
    std::cout << "PASS " << check << '\n';
}
void pins(const simnodus::FixturePreviewPins& rows)
{
    graph_probe::array(rows, [](const auto& row) {
        std::cout << "{\"logical_pin\":"; graph_probe::quote(row.logical_pin);
        std::cout << ",\"symbol_pin\":"; graph_probe::quote(row.symbol_pin);
        std::cout << ",\"x\":" << row.x << ",\"y\":" << row.y << '}';
    });
}
template<class Result> bool graph_result(const Result& result)
{
    return std::holds_alternative<std::shared_ptr<const simnodus::ProjectGraph>>(result);
}
int lifecycle(const std::string& root)
{
#ifndef _WIN32
    (void)root;
    std::cout << "SKIP Windows local NTFS fixture artwork lifecycle\n";
    return 0;
#else
    simnodus::EditorDocument document;
    require(graph_result(document.open(root, "original.json")), "inert document open without other lock files");
    const auto original = document.graph();
    const auto bytes = std::string(document.bytes());
    require(!document.dirty() && !document.can_undo() && !document.can_redo(), "clean document before explicit Preview");
    const auto preview = simnodus::capture_fixture_symbol(*original, "resistor", root);
    require(std::holds_alternative<std::shared_ptr<const simnodus::SymbolPreviewCapture>>(preview), "explicit native symbol capture");
    const auto capture = std::get<0>(preview);
    const auto& selected = capture->selection;
    require(capture->pins && (*capture->pins)[0].logical_pin == "p" && (*capture->pins)[0].symbol_pin == "a" &&
        (*capture->pins)[1].logical_pin == "n" && (*capture->pins)[1].symbol_pin == "b", "owned declared pins follow closed fixture convention");
    require(capture->resources->size() == 1 && capture->resources->front().data.size() == 227 && selected.request.resource == "symbol",
        "exactly one owned selected symbol");
    require(!std::filesystem::exists(std::filesystem::path(root) / "LICENSE") &&
        !std::filesystem::exists(std::filesystem::path(root) / "tests/schema/fixtures/assets/passive.cir"), "model and license remain absent");
    require(document.graph() == original && document.bytes() == bytes && !document.dirty() && !document.can_undo(), "Preview creates no document mutation or history");
    require(!original->declaration->sources.lock.resources_verified && !original->declaration->sources.lock.containment_verified &&
        !original->declaration->sources.resource_interfaces_verified && !original->declaration->runtime_profile_verified,
        "single capture raises no global resource or runtime flags");
    require(graph_result(document.set_resistance("main", "right", "3.5")) && graph_result(document.rename("Fixture preview project")),
        "existing independent edits remain available");
    const auto edited = document.graph();
    require(graph_result(document.undo_edit()) && graph_result(document.redo_edit()) && document.graph() == edited,
        "existing one-step history remains exact");
    require(std::get<0>(simnodus::select_fixture_symbol(*edited, "resistor")).pin_map == selected.pin_map &&
        std::get<0>(simnodus::inspect_fixture_pins(*edited, "resistor")) == *capture->pins,
        "equivalent complete mapping survives independent edits and history");
    require(std::holds_alternative<simnodus::ProjectSaveReceipt>(document.save_copy("preview-copy.json")) && document.dirty() &&
        document.leaf() == "original.json", "explicit copy retains association and dirty state");
    const auto path = std::filesystem::path(root) / selected.request.path;
    { std::ofstream changed(path, std::ios::binary | std::ios::trunc); changed << "replaced fixture source"; }
    const auto& data = capture->resources->front().data;
    require(simnodus::symbols::recognize_fixture_artwork(std::string_view(reinterpret_cast<const char*>(data.data()), data.size())).has_value(),
        "captured bytes survive source replacement");
    require(std::holds_alternative<simnodus::SymbolPreviewError>(simnodus::capture_fixture_symbol(*edited, "resistor", root)) &&
        document.graph() == edited && document.can_undo(), "failed recapture retains independent document/history");
    document = simnodus::EditorDocument{};
    require(capture->pins && (*capture->pins)[0].x == 0 && (*capture->pins)[1].x == 100 &&
        selected.pin_map.size() == 2 && selected.pin_map[0].logical_pin == "n", "owned full map and positions survive owner release");
    require(capture->selection.component == "resistor" && capture->requested_root == root && capture->artwork.lines.size() == 6 &&
        capture->resources->front().sha256 == selected.request.sha256, "capture owns metadata bytes root spelling and geometry");
    return 0;
#endif
}
}
int main(int argc, char** argv)
{
    try {
        if(argc < 2) return 2;
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        const std::string mode = argv[1];
        if(mode == "--lifecycle" && argc == 3) return lifecycle(argv[2]);
        std::string raw(simnodus::declaration_max_bytes + 1, '\0');
        std::cin.read(raw.data(), static_cast<std::streamsize>(raw.size()));
        raw.resize(static_cast<std::size_t>(std::cin.gcount()));
        if(mode == "--decode") {
            const auto artwork = simnodus::symbols::recognize_fixture_artwork(raw);
            if(!artwork) return error({simnodus::SymbolPreviewStage::artwork, "artwork"});
            raw.clear(); geometry(*artwork); std::cout << '\n'; return 0;
        }
        if(argc < 3) return 2;
        auto loaded = simnodus::load_project_graph(raw);
        if(const auto* e = std::get_if<simnodus::LockError>(&loaded))
            return error({simnodus::SymbolPreviewStage::selection, e->code});
        auto graph = std::get<0>(loaded);
        raw.assign(raw.size(), 'x');
        if(mode == "--pins") {
            const auto result = simnodus::inspect_fixture_pins(*graph, argv[2]);
            if(const auto* e = std::get_if<simnodus::SymbolPreviewError>(&result)) return error(*e);
            graph.reset(); loaded = simnodus::LockError{"test-owner-released", 0};
            pins(std::get<0>(result)); std::cout << '\n'; return 0;
        }
        if(mode == "--select") {
            const auto selected = simnodus::select_fixture_symbol(*graph, argv[2]);
            if(const auto* e = std::get_if<simnodus::SymbolPreviewError>(&selected)) return error(*e);
            graph.reset(); loaded = simnodus::LockError{"test-owner-released", 0};
            selection(std::get<0>(selected)); std::cout << '\n'; return 0;
        }
        if((mode != "--capture" && mode != "--pin-capture") || argc != 4) return 2;
        const auto captured = simnodus::capture_fixture_symbol(*graph, argv[2], argv[3]);
        if(const auto* e = std::get_if<simnodus::SymbolPreviewError>(&captured)) return error(*e);
        graph.reset(); loaded = simnodus::LockError{"test-owner-released", 0};
        const auto& value = *std::get<0>(captured);
        if(mode == "--pin-capture") {
            std::cout << "{\"pins\":";
            if(value.pins) pins(*value.pins); else std::cout << "null";
            std::cout << ",\"pin_map\":";
            graph_probe::array(value.selection.pin_map, [](const auto& row) {
                std::cout << '['; graph_probe::quote(row.logical_pin); std::cout << ','; graph_probe::quote(row.symbol_pin); std::cout << ']';
            });
            std::cout << ",\"captured_files\":" << value.resources->size() << "}\n";
            return 0;
        }
        std::cout << "{\"selection\":"; selection(value.selection);
        std::cout << ",\"geometry\":"; geometry(value.artwork);
        std::cout << ",\"captured_files\":" << value.resources->size() << ",\"captured_bytes\":" << value.resources->front().data.size() << "}\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
