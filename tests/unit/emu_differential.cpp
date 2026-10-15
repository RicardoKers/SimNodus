// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Differential test of the SimNodus emulator against Renode (E-07, gate G0a.2).
// Runs batches made by tests/experiments/emu-differential/generate.py in both
// and compares the 32 result words of every block (R0-R12, LR, APSR, 16
// scratch words, PRIMASK). Prints every difference and the decoder coverage.
//
//   emu_differential <renode> <renode-resources> <diff.repl> <blocks> <batch.elf>...
#include "emu/bus.hpp"
#include "emu/cpu.hpp"
#include "emu/elf.hpp"
#include "mcu/renode_engine.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace simnodus;
namespace fs = std::filesystem;

namespace {

constexpr std::uint32_t done_address = 0x20000000;
constexpr std::uint32_t results = 0x20010000;
constexpr std::uint32_t result_words = 32;
constexpr std::uint32_t done_marker = 0xD1FFD1FF;
constexpr std::uint32_t fault_marker = 0xFA017FA0;

// Word i of a result slot (push order: the last pushed is at the lowest address).
const char* word_name(unsigned i)
{
    static const char* names[32] = {"PRIMASK", "s48", "s52", "s56", "s60", "s32", "s36", "s40", "s44", "s0", "s4", "s8",
        "s12", "s16", "s20", "s24", "s28", "APSR", "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9",
        "R10", "R11", "R12", "LR"};
    return names[i];
}

struct Run {
    std::vector<std::uint32_t> words; // blocks * 32
    bool ok = false;
    std::string error;
    double seconds = 0;
};

Run run_emulator(const emu::ElfImage& image, unsigned blocks, std::vector<std::uint64_t>& hits)
{
    Run run;
    emu::Bus bus;
    bus.add_memory("SRAM", 0x20000000, 0x100000, true);
    bus.add_memory("flash", 0x08000000, 0x100000, false);
    bus.add_alias("boot alias", 0, "flash");
    emu::Cpu cpu(bus);
    bus.add_device(0xE000E000, 0x1000, &cpu.scs());
    for (const auto& segment : image.segments) {
        bus.load(segment.physical, segment.data.data(), static_cast<std::uint32_t>(segment.data.size()));
    }
    cpu.count_ops(true);
    cpu.reset();
    const auto begin = std::chrono::steady_clock::now();
    std::uint32_t done = 0;
    while (done != done_marker && done != fault_marker && cpu.state() == emu::CoreState::Running) {
        cpu.run(cpu.cycles() + 1000000);
        bus.read(done_address, 4, done);
        if (cpu.cycles() > 2000000000ull) {
            break;
        }
    }
    run.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    if (done != done_marker) {
        run.error = done == fault_marker ? "the emulator took a fault: " + cpu.last_fault() : "the emulator did not finish";
        return run;
    }
    run.words.resize(std::size_t{blocks} * result_words);
    for (std::size_t i = 0; i < run.words.size(); ++i) {
        bus.read(results + static_cast<std::uint32_t>(i * 4), 4, run.words[i]);
    }
    const auto& counts = cpu.op_hits();
    if (hits.size() < counts.size()) {
        hits.resize(counts.size(), 0);
    }
    for (std::size_t i = 0; i < counts.size(); ++i) {
        hits[i] += counts[i];
    }
    run.ok = true;
    return run;
}

Run run_renode(const fs::path& renode, const fs::path& resources, const fs::path& elf, unsigned blocks)
{
    Run run;
    mcu::MachineOptions options;
    options.renode = renode;
    options.resources = resources;
    options.platform = "diff.repl";
    options.extensions = {};
    options.firmware = elf;
    options.after_firmware = {"sysbus.cpu VectorTableOffset 0x08000000"};
    options.start_timeout = std::chrono::milliseconds(180000);
    std::string error;
    auto engine = mcu::RenodeEngine::start(options, error);
    if (!engine) {
        run.error = "Renode: " + error;
        return run;
    }
    const auto begin = std::chrono::steady_clock::now();
    std::uint32_t done = 0;
    for (int i = 0; i < 400 && done != done_marker && done != fault_marker; ++i) {
        if (!engine->run_for(10000)) {
            run.error = "Renode run failed: " + engine->error();
            return run;
        }
        done = engine->read32(done_address).value_or(0);
    }
    if (done != done_marker) {
        run.error = done == fault_marker ? "Renode took a fault" : "Renode did not finish";
        return run;
    }
    run.words.resize(std::size_t{blocks} * result_words);
    for (std::size_t i = 0; i < run.words.size(); ++i) {
        const auto value = engine->read32(results + static_cast<std::uint32_t>(i * 4));
        if (!value) {
            run.error = "Renode read failed";
            return run;
        }
        run.words[i] = *value;
    }
    run.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    engine->stop();
    run.ok = true;
    return run;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 6) {
        std::printf("usage: emu_differential <renode> <renode-resources> <diff.repl> <blocks> <batch.elf>...\n");
        return 2;
    }
    const fs::path renode = argv[1];
    const fs::path resources = argv[2];
    const fs::path repl = argv[3];
    const unsigned blocks = static_cast<unsigned>(std::strtoul(argv[4], nullptr, 10));

    // Renode reads LoopbackControl.cs and the platform from one folder.
    const fs::path folder = fs::temp_directory_path() / "SimNodus tests" / "emu-differential";
    fs::create_directories(folder);
    fs::copy_file(resources / "LoopbackControl.cs", folder / "LoopbackControl.cs", fs::copy_options::overwrite_existing);
    fs::copy_file(repl, folder / "diff.repl", fs::copy_options::overwrite_existing);

    std::vector<std::uint64_t> hits;
    unsigned compared = 0;
    unsigned differing_blocks = 0;
    for (int a = 5; a < argc; ++a) {
        const fs::path elf = argv[a];
        std::string error;
        const auto image = emu::read_elf(elf, error);
        if (!image) {
            std::printf("%s: %s\n", elf.string().c_str(), error.c_str());
            return 1;
        }
        const Run ours = run_emulator(*image, blocks, hits);
        const Run theirs = run_renode(renode, folder, elf, blocks);
        if (!ours.ok || !theirs.ok) {
            std::printf("%s: %s %s\n", elf.filename().string().c_str(), ours.error.c_str(), theirs.error.c_str());
            return 1;
        }
        unsigned differences = 0;
        for (unsigned b = 0; b < blocks; ++b) {
            bool block_differs = false;
            for (unsigned w = 0; w < result_words; ++w) {
                const std::uint32_t x = ours.words[b * result_words + w];
                const std::uint32_t y = theirs.words[b * result_words + w];
                if (x != y) {
                    if (differences < 60) {
                        std::printf("  %s block b%u %-5s emulator 0x%08X  Renode 0x%08X\n",
                            elf.filename().string().c_str(), b, word_name(w), static_cast<unsigned>(x),
                            static_cast<unsigned>(y));
                    }
                    ++differences;
                    block_differs = true;
                }
            }
            differing_blocks += block_differs ? 1 : 0;
        }
        compared += blocks;
        std::printf("%s: %u blocks, %u differing words (emulator %.2f s, Renode %.2f s)\n",
            elf.filename().string().c_str(), blocks, differences, ours.seconds, theirs.seconds);
    }
    std::printf("decoder coverage (emulator executions per operation):\n");
    for (std::size_t op = 1; op < hits.size(); ++op) {
        std::printf("  %-28s %llu\n", emu::Cpu::op_name(static_cast<std::uint8_t>(op)),
            static_cast<unsigned long long>(hits[op]));
    }
    std::printf("%u blocks compared, %u with differences\n", compared, differing_blocks);
    return differing_blocks == 0 ? 0 : 1;
}
