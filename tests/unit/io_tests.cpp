// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "io/project_file.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace simnodus;
using namespace simnodus::model;

namespace {

Schematic sample()
{
    Schematic s;
    Component r;
    r.id = s.allocate_id();
    r.type = "resistor";
    r.designator = "R1";
    r.position = {10, -4};
    r.orientation = {90, true};
    r.parameters = {{"R", "4k7"}};
    r.value_offset = Point{3, 2};
    r.models = {{"model", ".model MY-R R(TC1=0)"}};
    s.put(r);
    s.put(Wire{s.allocate_id(), {{13, -4}, {20, -4}, {20, 3}}});
    s.put(NetLabel{s.allocate_id(), {20, 3}, "OUT"});
    s.put(Bus{s.allocate_id(), {{0, 20}, {10, 20}}, "D"});
    s.put(Splitter{s.allocate_id(), {0, 20}, {90, true}, "D", 4, 3});
    s.put(TextNote{s.allocate_id(), {0, -10}, "Close SW1, then watch V(OUT).\nSecond line: \"quoted\" text"});
    Probe p;
    p.id = s.allocate_id();
    p.kind = ProbeKind::Current;
    p.target = {r.id, "1", {}};
    p.color = "#1f77b4";
    s.put(p);
    Probe v;
    v.id = s.allocate_id();
    v.target = {2, "", {20, 0}};
    v.reference = Anchor{r.id, "2", {}};
    s.put(v);
    SimulationSettings settings;
    settings.mode = SimulationMode::Transient;
    settings.transient_stop = 0.02;
    s.set_settings(settings);
    s.allocate_id(); // a gap must survive: IDs are never reused
    return s;
}

std::filesystem::path temp_dir()
{
    auto dir = std::filesystem::temp_directory_path() / ("simnodus-io-test-" + std::to_string(std::rand()));
    std::filesystem::create_directories(dir);
    return dir;
}

std::string read(const std::filesystem::path& file)
{
    std::ifstream input(file, std::ios::binary);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

} // namespace

TEST_CASE("projects round-trip through JSON")
{
    const Schematic original = sample();
    const std::string text = io::to_json_text(original);
    CHECK(text.find("\"format\": \"simnodus-project\"") != std::string::npos);
    const auto loaded = io::from_json_text(text);
    for (const auto& e : loaded.errors) {
        std::printf("  %s\n", e.c_str());
    }
    REQUIRE(loaded.schematic.has_value());
    CHECK(*loaded.schematic == original);
    CHECK(loaded.schematic->next_id() == original.next_id());
    CHECK(io::to_json_text(*loaded.schematic) == text); // stable output
}

TEST_CASE("invalid project files are rejected with a message")
{
    CHECK(!io::from_json_text("{").schematic);
    CHECK(!io::from_json_text("{\"format\": \"other\"}").schematic);
    const auto newer = io::from_json_text("{\"format\": \"simnodus-project\", \"version\": 99}");
    REQUIRE(!newer.errors.empty());
    CHECK(newer.errors[0].find("newer") != std::string::npos);
    const auto duplicate = io::from_json_text(R"({"format": "simnodus-project", "version": 1,
        "components": [{"id": 1, "type": "r", "designator": "R1", "at": [0, 0]}],
        "wires": [{"id": 1, "points": [[0, 0], [1, 0]]}]})");
    CHECK(!duplicate.schematic);
    const auto bad_point = io::from_json_text(R"({"format": "simnodus-project", "version": 1,
        "wires": [{"id": 1, "points": [[0, 0.5], [1, 0]]}]})");
    REQUIRE(!bad_point.schematic);
    CHECK(bad_point.errors[0].find("wires entry 1") != std::string::npos);
    const auto bad_setting = io::from_json_text(R"({"format": "simnodus-project", "version": 1,
        "settings": {"max_step": -1}})");
    CHECK(bad_setting.schematic.has_value());
    CHECK(bad_setting.warnings.size() == 1);
}

TEST_CASE("saving is atomic and keeps a backup")
{
    const auto dir = temp_dir();
    const auto file = dir / "lesson.snx";
    Schematic first = sample();
    auto saved = io::save_project(first, file);
    CHECK(saved.ok);
    CHECK(saved.backup.empty());
    Schematic second = first;
    second.erase(1);
    saved = io::save_project(second, file);
    CHECK(saved.ok);
    CHECK(saved.backup == std::filesystem::path(file.string() + ".bak"));
    CHECK(io::load_project(saved.backup).schematic == first);
    CHECK(io::load_project(file).schematic == second);
    // No temporary files are left behind.
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        (void)entry;
        ++files;
    }
    CHECK(files == 2);
    CHECK(read(file) == io::to_json_text(second));
    CHECK(!io::save_project(first, dir / "missing-folder" / "x.snx").ok);
    CHECK(!io::load_project(dir / "nothing.snx").schematic);
    std::filesystem::remove_all(dir);
}

TEST_CASE("clipboard text holds selected elements")
{
    const Schematic s = sample();
    const std::string text = io::elements_to_json_text({*s.get(1), *s.get(2)});
    std::string error;
    const auto elements = io::elements_from_json_text(text, error);
    REQUIRE(elements.has_value());
    CHECK(elements->size() == 2);
    CHECK(!io::elements_from_json_text(io::to_json_text(s), error)); // a project is not a clipboard
    CHECK(!io::elements_from_json_text("garbage", error));
}

TEST_CASE("exact edges are saved; old projects and wrong values keep the default")
{
    Schematic s;
    SimulationSettings settings;
    settings.exact_edges = true;
    s.set_settings(settings);
    auto loaded = io::from_json_text(io::to_json_text(s));
    REQUIRE(loaded.schematic.has_value());
    CHECK(loaded.schematic->settings().exact_edges);

    std::string text = io::to_json_text(s);
    const auto at = text.find("\"exact_edges\"");
    REQUIRE(at != std::string::npos);
    const auto end = text.find_first_of(",}", at);
    text.replace(at, end - at, "\"exact_edges\": 1");
    loaded = io::from_json_text(text);
    REQUIRE(loaded.schematic.has_value());
    CHECK(!loaded.schematic->settings().exact_edges);
    CHECK(!loaded.warnings.empty());

    const std::string wrong = "\"exact_edges\": 1";
    text.replace(text.find(wrong), wrong.size(), "\"speed\": 1");
    loaded = io::from_json_text(text); // a project from before the setting
    REQUIRE(loaded.schematic.has_value());
    CHECK(!loaded.schematic->settings().exact_edges);
}

TEST_CASE("the microcontroller quantum is saved and checked")
{
    Schematic s;
    SimulationSettings settings;
    settings.mcu_quantum = 1e-4;
    s.set_settings(settings);
    const auto loaded = io::from_json_text(io::to_json_text(s));
    REQUIRE(loaded.schematic.has_value());
    CHECK(loaded.schematic->settings().mcu_quantum == 1e-4);

    std::string text = io::to_json_text(s);
    const auto at = text.find("\"mcu_quantum\"");
    REQUIRE(at != std::string::npos);
    const auto end = text.find_first_of(",}", at);
    text.replace(at, end - at, "\"mcu_quantum\": 5");
    const auto out_of_range = io::from_json_text(text);
    REQUIRE(out_of_range.schematic.has_value());
    CHECK(out_of_range.schematic->settings().mcu_quantum == SimulationSettings{}.mcu_quantum);
    CHECK(!out_of_range.warnings.empty());
}
