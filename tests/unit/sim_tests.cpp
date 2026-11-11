// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "sim/chart_record.hpp"
#include "sim/measurements.hpp"
#include "sim/ngspice_runtime.hpp"
#include "sim/process_memory.hpp"
#include "sim/protocol_decode.hpp"
#include "sim/sample_ring.hpp"
#include "sim/vcd.hpp"

#include <algorithm>
#include <cmath>

using namespace simnodus::sim;

TEST_CASE("sample ring keeps the newest samples in order")
{
    SampleRing ring(3);
    CHECK(ring.snapshot().empty());
    for (int i = 0; i < 5; ++i) {
        ring.push(i * 0.1, i);
    }
    const auto samples = ring.snapshot();
    REQUIRE(samples.size() == 3);
    CHECK(samples[0].value == 2.0);
    CHECK(samples[1].value == 3.0);
    CHECK(samples[2].value == 4.0);
    CHECK(samples[2].time == 0.4);
    CHECK(ring.dropped() == 2);
    ring.clear();
    CHECK(ring.size() == 0);
    CHECK(ring.dropped() == 0);
}

TEST_CASE("sample ring returns the recent part")
{
    SampleRing ring(5);
    for (int i = 0; i < 8; ++i) {
        ring.push(i, i);
    }
    const auto recent = ring.since(5.5); // ring holds 3..7
    REQUIRE(recent.size() == 3);       // 5 (just before), 6, 7
    CHECK(recent[0].time == 5.0);
    CHECK(recent[2].time == 7.0);
    CHECK(ring.since(100).size() == 1);
    CHECK(ring.since(-1).size() == 5);
}

TEST_CASE("sample ring with zero capacity holds one sample")
{
    SampleRing ring(0);
    ring.push(1.0, 1.0);
    ring.push(2.0, 2.0);
    CHECK(ring.capacity() == 1);
    REQUIRE(ring.snapshot().size() == 1);
    CHECK(ring.snapshot()[0].value == 2.0);
}

TEST_CASE("ngspice version is parsed from engine output")
{
    CHECK(parse_ngspice_version("** ngspice-47 : Circuit level simulation program") == 47);
    CHECK(parse_ngspice_version("stdout ******\nstdout ** ngspice-42 : Circuit") == 42);
    CHECK(!parse_ngspice_version("no version here").has_value());
}

TEST_CASE("bundled ngspice is the first candidate")
{
    const auto candidates = ngspice_candidates("app");
    REQUIRE(!candidates.empty());
    CHECK(candidates.front().scripts_dir == std::filesystem::path("app") / "ngspice");
#ifdef _WIN32
    CHECK(candidates.front().library == std::filesystem::path("app") / "ngspice" / "ngspice.dll");
    CHECK(candidates.size() == 1);
#else
    CHECK(candidates.front().library == std::filesystem::path("app") / "ngspice" / "libngspice.so.0");
#endif
}

TEST_CASE("process memory query returns a resident size")
{
    const auto memory = current_process_memory();
    CHECK(memory.resident_bytes > 0);
}

TEST_CASE("measurements of a square wave")
{
    // 1 kHz, 0..5 V, 25 % duty, sampled every 1 us with 1 us edges.
    std::vector<Sample> s;
    for (int i = 0; i <= 10000; ++i) {
        const double t = i * 1e-6;
        const double phase = std::fmod(t, 1e-3);
        s.push_back({t, phase < 0.25e-3 ? 5.0 : 0.0});
    }
    const auto m = measure(s, 0, 10e-3);
    REQUIRE(m.has_value());
    CHECK(m->min == 0.0);
    CHECK(m->max == 5.0);
    CHECK(m->peak_to_peak == 5.0);
    CHECK(std::abs(m->mean - 1.25) < 0.01);
    CHECK(std::abs(m->rms - 5.0 * std::sqrt(0.25)) < 0.02);
    REQUIRE(m->frequency.has_value());
    CHECK(std::abs(*m->frequency - 1000.0) < 1.0);
    REQUIRE(m->duty.has_value());
    CHECK(std::abs(*m->duty - 0.25) < 0.01);
    CHECK(!measure(s, 1.0, 2.0).has_value());
}

TEST_CASE("measurements of a sine and a constant")
{
    std::vector<Sample> s;
    for (int i = 0; i <= 20000; ++i) {
        const double t = i * 1e-6;
        s.push_back({t, 2.0 * std::sin(2 * 3.14159265358979 * 500 * t)});
    }
    const auto m = measure(s, 0, 20e-3);
    REQUIRE(m.has_value());
    CHECK(std::abs(m->rms - 2.0 / std::sqrt(2.0)) < 1e-3);
    CHECK(std::abs(m->mean) < 1e-3);
    CHECK(std::abs(m->frequency.value_or(0) - 500.0) < 0.5);
    const auto flat = measure({{0, 3.3}, {1, 3.3}}, 0, 1);
    REQUIRE(flat.has_value());
    CHECK(!flat->frequency.has_value());
    CHECK(flat->rms == 3.3);
}

TEST_CASE("trigger finds the last crossing")
{
    const std::vector<Sample> s{{0, 0}, {1, 2}, {2, 0}, {3, 2}, {4, 0}};
    CHECK(std::abs(find_trigger(s, 1.0, true, 0, 4).value_or(-1) - 2.5) < 1e-12);
    CHECK(std::abs(find_trigger(s, 1.0, false, 0, 4).value_or(-1) - 3.5) < 1e-12);
    CHECK(std::abs(find_trigger(s, 1.0, true, 0, 2).value_or(-1) - 0.5) < 1e-12);
    CHECK(!find_trigger(s, 5.0, true, 0, 4).has_value());
    CHECK(std::abs(value_at(s, 0.5).value_or(-1) - 1.0) < 1e-12);
    CHECK(!value_at(s, 9).has_value());
}

TEST_CASE("VCD names keep letters, digits and underscores")
{
    CHECK(vcd_name("U1.PA0") == "U1_PA0");
    CHECK(vcd_name("V(OUT)") == "V_OUT");
    CHECK(vcd_name("V(A,B)") == "V_A_B");
    CHECK(vcd_name("()") == "signal");
}

TEST_CASE("VCD lists the selected signals and their value changes")
{
    const std::vector<VcdSignal> signals{
        {"U1.PC13", true, {{500, false}, {2000, true}, {3000, true}}},
        {"V(OUT)", false, {{1500, true}, {2000, false}, {2000, true}}},
        {"U1.PC13", false, {}},
    };
    const std::string expected = "$version SimNodus 0.3.0 $end\n"
                                 "$comment interactive run $end\n"
                                 "$timescale 1 ns $end\n"
                                 "$scope module simnodus $end\n"
                                 "$var wire 1 ! U1_PC13 $end\n"
                                 "$var wire 1 \" V_OUT $end\n"
                                 "$var wire 1 # U1_PC13_2 $end\n"
                                 "$upscope $end\n"
                                 "$enddefinitions $end\n"
                                 "#1000\n"
                                 "$dumpvars\n"
                                 "0!\n"
                                 "0\"\n"
                                 "0#\n"
                                 "$end\n"
                                 "#1500\n"
                                 "1\"\n"
                                 "#2000\n"
                                 "1!\n";
    // The change at 500 ns falls before the start and sets the initial level;
    // at 2000 ns V(OUT) ends where it was, so only U1.PC13 is written.
    CHECK(write_vcd(signals, 1000, "SimNodus 0.3.0", "interactive run") == expected);
}

int main(int argc, char** argv)
{
    return simnodus::test::run_all(argc > 1 ? argv[1] : "");
}

TEST_CASE("logic lane timing: period, pulse widths and duty cycle")
{
    // Low at first; high from 1 to 2 ms, 3 to 4 ms, 5 to 6 ms, 7 to 8 ms.
    std::vector<std::pair<double, bool>> changes;
    for (int k = 0; k < 4; ++k) {
        changes.push_back({(1 + 2 * k) * 1e-3, true});
        changes.push_back({(2 + 2 * k) * 1e-3, false});
    }
    const auto m = measure_logic(false, changes, 0, 10e-3);
    CHECK(m.rising == 4);
    CHECK(m.falling == 4);
    REQUIRE((m.period && m.frequency && m.high_time && m.low_time && m.duty));
    CHECK(std::abs(*m.period - 2e-3) < 1e-12);
    CHECK(std::abs(*m.frequency - 500.0) < 1e-6);
    CHECK(std::abs(*m.high_time - 1e-3) < 1e-12);
    CHECK(std::abs(*m.low_time - 1e-3) < 1e-12); // complete low pulses only (2..3, 4..5, 6..7 ms)
    CHECK(std::abs(*m.duty - 0.4) < 1e-12);
    CHECK(logic_level_at(false, changes, 1.5e-3));
    CHECK(!logic_level_at(false, changes, 2.5e-3));
    const auto part = measure_logic(false, changes, 1.5e-3, 3.5e-3); // starts high
    CHECK(part.rising == 1);
    CHECK(part.falling == 1);
    CHECK(!part.period);
    CHECK(std::abs(*part.duty - 0.5) < 1e-12);
}

TEST_CASE("thinning keeps peaks, edges and the time order")
{
    SampleRing ring(100000);
    for (int i = 0; i < 100000; ++i) {
        const double t = i * 1e-4;
        double v = (i / 500) % 2 ? 5.0 : 0.0; // 10 Hz square wave
        if (i == 33333) {
            v = 9.0; // a single-sample spike
        }
        ring.push(t, v);
    }
    const auto thin = ring.since(0.0, 0.01); // 10 s in 1000 buckets
    CHECK(thin.size() <= 4004);
    CHECK(thin.size() > 1000);
    // The same buckets in a later window: the shared part is thinned the same way.
    const auto later = ring.since(2.0, 0.01);
    const auto shared = std::find_if(thin.begin(), thin.end(), [&](const Sample& s) { return s.time >= later[1].time; });
    REQUIRE(shared != thin.end());
    CHECK(shared->time == later[1].time);
    CHECK(shared->value == later[1].value);
    bool spike = false;
    for (std::size_t i = 1; i < thin.size(); ++i) {
        CHECK(thin[i].time >= thin[i - 1].time);
        spike = spike || thin[i].value == 9.0;
    }
    CHECK(spike);
    bool initial = true;
    const auto edges = ring.transitions(0.0, 2.5, initial);
    CHECK(!initial);
    CHECK(edges.size() == 199 + 2); // 199 square-wave edges; the spike rises from 0 V and falls back
}

TEST_CASE("chart record keeps the whole run in its buckets")
{
    ChartRecord record(8, 1.0);
    CHECK(record.empty());
    for (int i = 0; i < 8; ++i) {
        record.add(i + 0.25, i);
        record.add(i + 0.75, -i);
    }
    REQUIRE(record.buckets().size() == 8);
    CHECK(record.buckets()[3].first == 3.0);
    CHECK(record.buckets()[3].last == -3.0);
    CHECK(record.buckets()[3].low == -3.0);
    CHECK(record.buckets()[3].high == 3.0);
    // Beyond the eight buckets: they merge in pairs and the bucket time doubles.
    record.add(8.5, 100.0);
    CHECK(record.bucket_time() == 2.0);
    REQUIRE(record.buckets().size() == 5);
    CHECK(record.buckets()[0].first == 0.0);
    CHECK(record.buckets()[0].last == -1.0);
    CHECK(record.buckets()[0].high == 1.0);
    CHECK(record.buckets()[4].last == 100.0);
    CHECK(record.latest()->value == 100.0);
    CHECK(record.end_time() == 8.5);
    // An older sample is ignored; an hour later still fits in the capacity.
    record.add(1.0, 50.0);
    CHECK(record.latest()->value == 100.0);
    record.add(3600.0, 7.0);
    CHECK(record.buckets().size() <= record.capacity());
    CHECK(record.buckets().back().last == 7.0);
    CHECK(record.buckets().front().first == 0.0);
}

TEST_CASE("chart difference interpolates the minus series")
{
    const std::vector<Sample> plus{{0.0, 5.0}, {1.0, 5.0}, {2.0, 5.0}, {3.0, 5.0}};
    const std::vector<Sample> minus{{0.5, 1.0}, {2.5, 3.0}};
    const auto d = difference(plus, minus);
    REQUIRE(d.size() == 4);
    CHECK(d[0].value == 4.0); // held before the first sample
    CHECK(std::abs(d[1].value - 3.5) < 1e-12);
    CHECK(std::abs(d[2].value - 2.5) < 1e-12);
    CHECK(d[3].value == 2.0); // held after the last
    CHECK(difference(plus, {}).size() == 4);
}

TEST_CASE("chart CSV has one row per time and empty cells outside a record")
{
    std::vector<ChartRecord> records(2, ChartRecord(16, 1.0));
    records[0].add(0.5, 1.0);
    records[0].add(2.5, 3.0);
    records[1].add(1.5, 10.0);
    const auto csv = chart_csv(records, {"CH1 (V)", "CH2 (V)"});
    CHECK(csv == "time (s),CH1 (V),CH2 (V)\n0.5,1,\n1.5,2,10\n2.5,3,\n");
}

namespace {

// Builds the lines of a bus from a list of levels, one step of `period` each.
struct Lines {
    std::vector<LogicSignal> signals;
    std::vector<bool> levels;
    double time = 0.0;
    double period = 1e-5;

    explicit Lines(std::vector<bool> initial) : signals(initial.size()), levels(initial)
    {
        for (std::size_t i = 0; i < initial.size(); ++i) {
            signals[i].initial = initial[i];
        }
    }
    void set(std::size_t line, bool level)
    {
        if (levels[line] != level) {
            levels[line] = level;
            signals[line].changes.emplace_back(time, level);
        }
    }
    void wait(double steps = 1.0) { time += steps * period; }
};

// I2C on lines 0 (SCL) and 1 (SDA).
void i2c_start(Lines& l)
{
    l.set(1, true);
    l.wait();
    l.set(0, true);
    l.wait();
    l.set(1, false);
    l.wait();
    l.set(0, false);
    l.wait();
}

void i2c_byte(Lines& l, unsigned value, bool ack)
{
    for (int i = 8; i >= 0; --i) {
        l.set(1, i > 0 ? ((value >> (i - 1)) & 1u) != 0 : !ack);
        l.wait();
        l.set(0, true);
        l.wait();
        l.set(0, false);
        l.wait();
    }
}

void i2c_stop(Lines& l)
{
    l.set(1, false);
    l.wait();
    l.set(0, true);
    l.wait();
    l.set(1, true);
    l.wait();
}

std::vector<std::string> texts(const std::vector<DecodedItem>& items)
{
    std::vector<std::string> out;
    for (const auto& item : items) {
        out.push_back(item.text);
    }
    return out;
}

void uart_frame(Lines& l, unsigned value, int data_bits = 8, char parity = 'N', bool good_stop = true)
{
    l.set(0, false); // start
    l.wait();
    int ones = 0;
    for (int i = 0; i < data_bits; ++i) {
        const bool bit = ((value >> i) & 1u) != 0;
        ones += bit ? 1 : 0;
        l.set(0, bit);
        l.wait();
    }
    if (parity != 'N') {
        l.set(0, (ones % 2 == 1) == (parity == 'E'));
        l.wait();
    }
    l.set(0, good_stop);
    l.wait();
    l.set(0, true);
    l.wait(3);
}

} // namespace

TEST_CASE("the I2C decoder reads starts, addresses, data, acknowledges and stops (SN-286)")
{
    Lines l({true, true});
    l.wait(5);
    // Write the word address 0x10 to the chip at 0x50, then read two bytes from it.
    i2c_start(l);
    i2c_byte(l, 0xA0, true);
    i2c_byte(l, 0x10, true);
    l.set(1, true);
    l.wait();
    l.set(0, true);
    l.wait();
    l.set(1, false); // repeated start
    l.wait();
    l.set(0, false);
    l.wait();
    i2c_byte(l, 0xA1, true);
    i2c_byte(l, 0x53, true);
    i2c_byte(l, 0x69, false);
    i2c_stop(l);
    // A chip that is not there.
    l.wait(5);
    i2c_start(l);
    i2c_byte(l, 0x78, false);
    i2c_stop(l);
    const auto items = decode_i2c(l.signals[0], l.signals[1]);
    CHECK((texts(items)
        == std::vector<std::string>{"S", "50 W A", "10 A", "Sr", "50 R A", "53 A", "69 N", "P", "S", "3C W N", "P"}));
    REQUIRE(items.size() == 11);
    CHECK(items[1].kind == DecodedItem::Kind::Address);
    CHECK(items[1].value == 0x50);
    CHECK(items[1].acknowledged);
    CHECK(items[6].value == 0x69);
    CHECK(!items[6].acknowledged);
    CHECK(items[1].start < items[1].end);
    CHECK(items[1].end <= items[2].start);
    // From the middle of a frame: nothing until the next start.
    LogicSignal scl = l.signals[0];
    LogicSignal sda = l.signals[1];
    const double from = items[2].start;
    auto cut = [from](LogicSignal& s) {
        s.initial = level_at(s, from);
        std::erase_if(s.changes, [from](const auto& c) { return c.first <= from; });
    };
    cut(scl);
    cut(sda);
    const auto later = texts(decode_i2c(scl, sda));
    REQUIRE(later.size() >= 2);
    CHECK(later[0] == "S");
    CHECK(later[1] == "50 R A");
}

TEST_CASE("the SPI decoder reads words in the four modes, with and without a select line (SN-286)")
{
    for (int mode = 0; mode < 4; ++mode) {
        const bool idle = (mode & 2) != 0;
        const bool phase = (mode & 1) != 0;
        // Lines: 0 clock, 1 MOSI, 2 MISO, 3 select (active low).
        Lines l({idle, false, false, true});
        l.wait(3);
        auto word = [&](unsigned out, unsigned in, int bits) {
            for (int i = bits - 1; i >= 0; --i) {
                if (phase) { // data changes on the leading edge, is read on the trailing one
                    l.set(0, !idle);
                    l.set(1, ((out >> i) & 1u) != 0);
                    l.set(2, ((in >> i) & 1u) != 0);
                    l.wait();
                    l.set(0, idle);
                    l.wait();
                } else {
                    l.set(1, ((out >> i) & 1u) != 0);
                    l.set(2, ((in >> i) & 1u) != 0);
                    l.wait();
                    l.set(0, !idle);
                    l.wait();
                    l.set(0, idle);
                }
            }
        };
        l.set(3, false);
        l.wait();
        word(0x9F, 0x00, 8);
        word(0x00, 0xEF, 8);
        l.wait();
        l.set(3, true);
        l.wait(4);
        // Three stray clocks while not selected, then a word cut short by the select line.
        word(0x5, 0x5, 3);
        l.wait();
        l.set(3, false);
        l.wait();
        word(0x1F, 0x1F, 5);
        l.wait();
        l.set(3, true);
        l.wait();
        l.set(3, false);
        l.wait();
        word(0xA5, 0x3C, 8);
        l.wait();
        l.set(3, true);
        SpiOptions options;
        options.mode = mode;
        options.use_select = true;
        const auto items = decode_spi(l.signals[0], l.signals[1], l.signals[2], l.signals[3], options);
        CHECK((texts(items) == std::vector<std::string>{"9F/00", "00/EF", "A5/3C"}));
        REQUIRE(items.size() == 3);
        CHECK(items[0].value == 0x9F);
        CHECK(items[1].second == 0xEF);
        CHECK(items[0].end <= items[1].start);
    }
    // 16-bit words, least significant bit first, MOSI alone, no select line.
    Lines l({false, false});
    l.wait();
    for (int i = 0; i < 16; ++i) {
        l.set(1, ((0x1234u >> i) & 1u) != 0);
        l.wait();
        l.set(0, true);
        l.wait();
        l.set(0, false);
    }
    SpiOptions options;
    options.bits = 16;
    options.msb_first = false;
    options.has_miso = false;
    CHECK((texts(decode_spi(l.signals[0], l.signals[1], {}, {}, options)) == std::vector<std::string>{"1234"}));
}

TEST_CASE("the UART decoder reads frames, finds the rate and reports errors (SN-286)")
{
    Lines l({true});
    l.period = 1.0 / 9600;
    l.wait(4);
    for (const char c : std::string("Hi\n")) {
        uart_frame(l, static_cast<unsigned char>(c));
    }
    uart_frame(l, 0x55, 8, 'N', false); // a stop bit held low
    l.set(0, true);
    l.wait(12);
    uart_frame(l, 0xFF); // only the start bit is low
    CHECK(uart_baud(l.signals[0]) == 9600);
    {
        // One short glitch before the frames (a pin becoming the transmitter's output) is not a bit.
        LogicSignal glitch = l.signals[0];
        glitch.changes.insert(glitch.changes.begin(), {{1e-6, false}, {2e-6, true}});
        CHECK(uart_baud(glitch) == 9600);
    }
    const auto items = decode_uart(l.signals[0], {});
    CHECK((texts(items) == std::vector<std::string>{"48 'H'", "69 'i'", "0A", "frame error", "FF"}));
    REQUIRE(items.size() == 5);
    CHECK(items[3].kind == DecodedItem::Kind::Error);
    CHECK(std::abs((items[0].end - items[0].start) - 10.0 / 9600) < 1e-9);

    // Seven data bits with even parity at a given rate; one frame with a wrong parity bit.
    Lines p({true});
    p.period = 1.0 / 115200;
    p.wait(3);
    uart_frame(p, 'A', 7, 'E');
    uart_frame(p, 'C', 7, 'O'); // odd parity sent to an even receiver
    UartOptions options;
    options.baud = 115200;
    options.data_bits = 7;
    options.parity = 'E';
    CHECK((texts(decode_uart(p.signals[0], options)) == std::vector<std::string>{"41 'A'", "43 parity error"}));

    // An inverted line idles low.
    Lines n({false});
    n.period = 1.0 / 9600;
    n.wait(3);
    n.set(0, true); // start
    n.wait();
    for (int i = 0; i < 8; ++i) {
        n.set(0, ((0x41u >> i) & 1u) == 0);
        n.wait();
    }
    n.set(0, false);
    n.wait(3);
    UartOptions inverted;
    inverted.baud = 9600;
    inverted.inverted = true;
    CHECK((texts(decode_uart(n.signals[0], inverted)) == std::vector<std::string>{"41 'A'"}));
    CHECK(decode_uart({}, {}).empty());
}
