// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// E-03 execution host. This is measured experiment code, not a production adapter.
extern "C" {
#include "renode_api.h"
}
#ifdef NO_ERROR
#undef NO_ERROR
#endif
#include "firmware_layout.h"
#include "mailbox.h"
#include "ngspice_host.hpp"
#include <ngspice/sharedspice.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr double high_threshold = 2.31;
constexpr double low_threshold = 0.99;
constexpr uint64_t final_time_us = 10000;

void require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

int consume(renode_error_t* error)
{
    if (!error) { return ERR_NO_ERROR; }
    const int code = error->code;
    std::cerr << "Backend diagnostic " << code << ": "
              << (error->message ? error->message : "(none)") << '\n';
    renode_free_error(error);
    return code;
}

void success(renode_error_t* error)
{
    require(consume(error) == ERR_NO_ERROR, "Renode operation failed");
}

template<class T> using Handle = std::unique_ptr<T, decltype(&std::free)>;

struct Session {
    renode_t* value = nullptr;
    ~Session() { if (value) { consume(renode_disconnect(&value)); } }
};

struct GpioEvent {
    uint64_t timestamp_us = 0, request_begin_us = 0, request_end_us = 0;
    int pin = 0;
    bool state = false, during_run = false;
};

struct SourceEdge {
    double time_s = 0.0;
    double analog_before_s = 0.0;
    uint64_t origin_us = 0;
    bool state = false, discovered_late = false;
};

struct Capture {
    std::array<GpioEvent, 1024> events{};
    size_t count = 0;
    uint64_t begin = 0, end = 0;
    bool running = false, overflow = false;
};

struct PinCapture { Capture* capture = nullptr; int pin = 0; };

void changed(void* opaque, renode_gpio_event_data_t* data) noexcept
{
    auto& pin = *static_cast<PinCapture*>(opaque);
    auto& capture = *pin.capture;
    if (capture.count == capture.events.size()) { capture.overflow = true; return; }
    capture.events[capture.count++] = {
        data->timestamp_us, capture.begin, capture.end, pin.pin, data->state, capture.running};
}

class Renode {
public:
    explicit Renode(const char* port)
    {
        success(renode_connect(port, &session_.value));
        renode_machine_t* machine = nullptr;
        success(renode_get_machine(session_.value, "sn014", &machine));
        machine_.reset(machine);
        renode_gpio_t* gpio = nullptr;
        success(renode_get_gpio(machine_.get(), "gpioPortA", &gpio));
        gpio_.reset(gpio);
        renode_bus_context_t* bus = nullptr;
        success(renode_get_sysbus(machine_.get(), &bus));
        bus_.reset(bus);
        for (auto& pin : pins_) {
            success(renode_register_gpio_state_change_callback(
                gpio_.get(), pin.pin, &pin, changed));
        }
    }

    uint64_t now() const
    {
        uint64_t result = 0;
        success(renode_get_current_time(session_.value, TU_MICROSECONDS, &result));
        return result;
    }

    void prepare()
    {
        require(now() == 0, "Renode did not start paused at zero");
        require(read(0) == 0x20005000 && read(0) == read(0x08000000)
                && read(4) == read(0x08000004), "Flash alias/vector map mismatch");
        write(SN_E03_DATA_ADDRESS, 0xaaaaaaaa);
        write(SN_E03_BSS_ADDRESS, 0xbbbbbbbb);
    }

    void verify_boot() const
    {
        const auto value = status();
        require(value.magic == SN_E03_BOOT_MAGIC, "Firmware did not boot");
        require(value.data_check == SN_E03_DATA_COOKIE && value.bss_check == 0,
                "Firmware startup data/BSS checks failed");
        require(value.fault == 0, "Firmware entered its fault handler");
    }

    void advance(uint64_t target)
    {
        require(target >= now(), "Renode target moved backward");
        if (target == now()) { return; }
        capture_.begin = now();
        capture_.end = target;
        capture_.running = true;
        auto* error = renode_run_for(session_.value, TU_MICROSECONDS, target - capture_.begin);
        capture_.running = false;
        success(error);
        require(now() == target, "RunFor did not reach its exact requested end");
        require(!capture_.overflow, "GPIO capture overflow");
    }

    int advance_expect_failure(uint64_t duration, const std::filesystem::path& marker)
    {
        capture_.begin = now();
        capture_.end = capture_.begin + duration;
        {
            std::ofstream ready(marker);
            ready << "request about to be submitted\n";
            require(ready.good(), "Cannot write failure synchronization marker");
        }
        capture_.running = true;
        auto* error = renode_run_for(session_.value, TU_MICROSECONDS, duration);
        capture_.running = false;
        if (!error) { return ERR_NO_ERROR; }
        const int code = error->code;
        consume(error);
        return code == ERR_NO_ERROR ? -1 : code;
    }

    void input(bool state)
    {
        const auto before = now();
        success(renode_set_gpio_state(gpio_.get(), 1, state));
        require(now() == before, "Input application advanced Renode time");
    }

    bool pin(int number) const
    {
        bool result = false;
        success(renode_get_gpio_state(gpio_.get(), number, &result));
        return result;
    }

    sn_e03_mailbox status() const
    {
        sn_e03_mailbox result{};
        success(renode_sysbus_read(
            bus_.get(), SN_E03_MAILBOX_ADDRESS, AW_MULTI_BYTE, &result, sizeof(result)));
        return result;
    }

    const Capture& capture() const { return capture_; }
    size_t event_count() const { return capture_.count; }

private:
    uint32_t read(uint64_t address) const
    {
        uint32_t result = 0;
        success(renode_sysbus_read(bus_.get(), address, AW_DOUBLE_WORD, &result, 1));
        return result;
    }

    void write(uint64_t address, uint32_t value)
    {
        success(renode_sysbus_write(bus_.get(), address, AW_DOUBLE_WORD, &value, 1));
    }

    Session session_;
    Handle<renode_machine_t> machine_{nullptr, &std::free};
    Handle<renode_gpio_t> gpio_{nullptr, &std::free};
    Handle<renode_bus_context_t> bus_{nullptr, &std::free};
    Capture capture_;
    std::array<PinCapture, 3> pins_{{
        {&capture_, 0}, {&capture_, 2}, {&capture_, 4}}};
};

struct AnalogSample { double time_s = 0.0, input_v = 0.0, output_v = 0.0; };
struct Stop { double requested_s = 0.0, actual_s = 0.0; };

struct AnalogApi {
    decltype(&ngSpice_Init) init;
    decltype(&ngSpice_Init_Sync) init_sync;
    decltype(&ngSpice_Command) command;
    decltype(&ngSpice_Circ) circuit;
    decltype(&ngSpice_SetBkpt) breakpoint;
};

class Analog {
public:
    explicit Analog(simnodus::experiment::Library& library)
        : api_{library.get<decltype(&ngSpice_Init)>("ngSpice_Init"),
               library.get<decltype(&ngSpice_Init_Sync)>("ngSpice_Init_Sync"),
               library.get<decltype(&ngSpice_Command)>("ngSpice_Command"),
               library.get<decltype(&ngSpice_Circ)>("ngSpice_Circ"),
               library.get<decltype(&ngSpice_SetBkpt)>("ngSpice_SetBkpt")}
    {
        require(api_.init(output, nullptr, exit_callback, data, init_data, background, this) == 0,
                "ngspice initialization failed");
        require(version_seen_ && init_disabled_, "Unexpected ngspice version/init state");
        require(api_.init_sync(source, nullptr, sync_callback, nullptr, this) == 0,
                "ngspice synchronization callback registration failed");
        schedule_.push_back({0.0, 0.0, 0, false, false});
    }

    void load(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        require(input.good(), "Missing E-03 netlist");
        std::vector<std::string> lines;
        for (std::string line; std::getline(input, line);) { lines.push_back(line); }
        std::vector<char*> pointers;
        for (auto& line : lines) { pointers.push_back(line.data()); }
        pointers.push_back(nullptr);
        require(api_.circuit(pointers.data()) == 0, "E-03 netlist load failed");
    }

    void add_edge(uint64_t origin_us, bool state, int64_t shift_ns)
    {
        const double effective = static_cast<double>(origin_us) * 1e-6
                               + static_cast<double>(shift_ns) * 1e-9;
        const double analog_before = samples_.empty() ? 0.0 : samples_.back().time_s;
        const bool discovered_late = effective + 1e-12 < analog_before;
        require(effective >= 0.0, "Shifted source edge became negative");
        require(effective >= schedule_.back().time_s, "Source schedule moved backward");
        if (effective == schedule_.back().time_s) {
            schedule_.back() = {effective, analog_before, origin_us, state, discovered_late};
        } else {
            schedule_.push_back({effective, analog_before, origin_us, state, discovered_late});
        }
        if (effective > analog_before + 1e-12) {
            require(api_.breakpoint(effective), "ngspice rejected a future source breakpoint");
        }
    }

    void advance(double target_s)
    {
        require(target_s > 0.0 && target_s <= 0.0100000001, "Invalid analog target");
        if (!started_) {
            command("stop when time > " + number(target_s));
            command("run");
            started_ = true;
        } else {
            command("delete all");
            command("stop when time > " + number(target_s));
            command("resume");
        }
        require(!samples_.empty(), "ngspice produced no accepted samples");
        const double actual = samples_.back().time_s;
        require(actual + 1e-12 >= target_s && actual <= target_s + 2e-6,
                "ngspice stop escaped the measured E-01 bound");
        stops_.push_back({target_s, actual});
    }

    void run_all()
    {
        command("run");
        started_ = true;
        require(!samples_.empty() && std::abs(samples_.back().time_s - 0.010) <= 1e-12,
                "ngspice replay did not finish at 10 ms");
        stops_.push_back({0.010, samples_.back().time_s});
    }

    void shutdown()
    {
        command("quit");
        require(exited_ && quit_ && exit_status_ == 0 && !callback_fault_,
                "ngspice shutdown or callback failed");
    }

    const std::vector<AnalogSample>& samples() const { return samples_; }
    const std::vector<SourceEdge>& schedule() const { return schedule_; }
    const std::vector<Stop>& stops() const { return stops_; }

private:
    static int output(char* message, int, void* user) noexcept
    {
        auto& self = *static_cast<Analog*>(user);
        try {
            std::puts(message);
            self.version_seen_ |= std::strstr(message, "ngspice-47") != nullptr;
            self.init_disabled_ |= std::strstr(message, "Note: .spiceinit is ignored") != nullptr;
            if (std::strstr(message, "Error") || std::strstr(message, "error")) { ++self.diagnostics_; }
        } catch (...) { self.callback_fault_ = true; }
        return 0;
    }

    static int exit_callback(int status, NG_BOOL, NG_BOOL quit, int, void* user) noexcept
    {
        auto& self = *static_cast<Analog*>(user);
        self.exited_ = true;
        self.exit_status_ = status;
        self.quit_ = quit;
        return 0;
    }

    static int data(pvecvaluesall values, int, int, void* user) noexcept
    {
        auto& self = *static_cast<Analog*>(user);
        try {
            require(self.samples_.size() < 200000, "Analog sample limit exceeded");
            const auto nan = std::numeric_limits<double>::quiet_NaN();
            AnalogSample point{nan, nan, nan};
            for (int index = 0; index < values->veccount; ++index) {
                const auto* value = values->vecsa[index];
                if (std::strcmp(value->name, "time") == 0) { point.time_s = value->creal; }
                if (std::strcmp(value->name, "in") == 0) { point.input_v = value->creal; }
                if (std::strcmp(value->name, "out") == 0) { point.output_v = value->creal; }
            }
            require(std::isfinite(point.time_s) && std::isfinite(point.input_v)
                    && std::isfinite(point.output_v), "Nonfinite analog callback data");
            self.samples_.push_back(point);
        } catch (...) { self.callback_fault_ = true; }
        return 0;
    }

    static int init_data(pvecinfoall, int, void*) noexcept { return 0; }

    static int source(double* value, double time, char*, int, void* user) noexcept
    {
        auto& self = *static_cast<Analog*>(user);
        try {
            auto edge = std::upper_bound(self.schedule_.begin(), self.schedule_.end(), time,
                [](double candidate, const SourceEdge& item) { return candidate < item.time_s; });
            if (edge != self.schedule_.begin()) { --edge; }
            *value = edge->state ? 3.3 : 0.0;
            ++self.source_queries_;
        } catch (...) {
            self.callback_fault_ = true;
            *value = 0.0;
        }
        return 0;
    }

    static int sync_callback(double, double*, double, int, int, int, void*) noexcept { return 0; }
    static int background(NG_BOOL, int, void*) noexcept { return 0; }

    static std::string number(double value)
    {
        std::ostringstream stream;
        stream << std::setprecision(17) << value;
        return stream.str();
    }

    void command(const std::string& text)
    {
        std::string mutable_text = text;
        const int result = api_.command(mutable_text.data());
        require(result == 0 || text == "quit", "ngspice command failed");
    }

    AnalogApi api_;
    std::vector<AnalogSample> samples_;
    std::vector<SourceEdge> schedule_;
    std::vector<Stop> stops_;
    bool version_seen_ = false, init_disabled_ = false, callback_fault_ = false;
    bool exited_ = false, quit_ = false, started_ = false;
    int exit_status_ = -1, diagnostics_ = 0;
    size_t source_queries_ = 0;
};

struct Crossing {
    double time_s = 0.0, observed_s = 0.0;
    uint64_t ceil_us = 0, apply_us = 0, consumer_before_us = 0;
    bool state = false, applied = false, late = false;
};

class CrossingDetector {
public:
    void scan(const Analog& analog)
    {
        const auto& samples = analog.samples();
        if (samples.size() < 2) { return; }
        if (next_ == 0) { next_ = 1; }
        for (; next_ < samples.size(); ++next_) {
            const auto& previous = samples[next_ - 1];
            const auto& current = samples[next_];
            const double threshold = state_ ? low_threshold : high_threshold;
            const bool crossed = state_
                ? previous.output_v > threshold && current.output_v <= threshold
                : previous.output_v < threshold && current.output_v >= threshold;
            if (!crossed) { continue; }
            const double ratio = (threshold - previous.output_v)
                               / (current.output_v - previous.output_v);
            const double time = previous.time_s + ratio * (current.time_s - previous.time_s);
            state_ = !state_;
            crossings_.push_back({time, samples.back().time_s,
                static_cast<uint64_t>(std::ceil(time * 1e6 - 1e-12)), 0, 0,
                state_, false, false});
        }
    }

    void apply_ready(Renode& renode)
    {
        for (auto& crossing : crossings_) {
            if (crossing.applied || crossing.ceil_us > renode.now()) { continue; }
            crossing.consumer_before_us = renode.now();
            crossing.apply_us = renode.now();
            crossing.late = crossing.apply_us > crossing.ceil_us;
            renode.input(crossing.state);
            crossing.applied = true;
        }
    }

    const std::vector<Crossing>& crossings() const { return crossings_; }

private:
    std::vector<Crossing> crossings_;
    size_t next_ = 0;
    bool state_ = false;
};

struct Boundary {
    uint64_t requested_us = 0, renode_actual_us = 0;
    double ngspice_actual_s = 0.0;
    bool exact_joint_boundary = false;
};

void write_common(const std::filesystem::path& directory, const Renode& renode,
                  const Analog& analog, const std::vector<Crossing>& crossings,
                  const std::vector<Boundary>& boundaries)
{
    std::ofstream gpio(directory / "gpio.csv");
    gpio << "pin,state,timestamp_us,request_begin_us,request_end_us,during_run\n";
    for (size_t index = 0; index < renode.capture().count; ++index) {
        const auto& event = renode.capture().events[index];
        gpio << event.pin << ',' << event.state << ',' << event.timestamp_us << ','
             << event.request_begin_us << ',' << event.request_end_us << ',' << event.during_run << '\n';
    }
    std::ofstream source(directory / "schedule.csv");
    source << std::setprecision(17)
           << "time_s,analog_before_s,origin_gpio_us,state,discovered_late\n";
    for (const auto& edge : analog.schedule()) {
        source << edge.time_s << ',' << edge.analog_before_s << ',' << edge.origin_us << ','
               << edge.state << ',' << edge.discovered_late << '\n';
    }
    std::ofstream samples(directory / "analog.csv");
    samples << std::setprecision(17) << "time_s,input_v,output_v\n";
    for (const auto& sample : analog.samples()) {
        samples << sample.time_s << ',' << sample.input_v << ',' << sample.output_v << '\n';
    }
    std::ofstream threshold(directory / "thresholds.csv");
    threshold << std::setprecision(17)
              << "state,crossing_s,observed_s,ceil_us,apply_us,consumer_before_us,applied,late\n";
    for (const auto& crossing : crossings) {
        threshold << crossing.state << ',' << crossing.time_s << ',' << crossing.observed_s << ','
                  << crossing.ceil_us << ',' << crossing.apply_us << ',' << crossing.consumer_before_us
                  << ',' << crossing.applied << ',' << crossing.late << '\n';
    }
    std::ofstream boundary(directory / "boundaries.csv");
    boundary << std::setprecision(17)
             << "requested_us,renode_actual_us,ngspice_actual_s,exact_joint_boundary\n";
    for (const auto& item : boundaries) {
        boundary << item.requested_us << ',' << item.renode_actual_us << ','
                 << item.ngspice_actual_s << ',' << item.exact_joint_boundary << '\n';
    }
    require(gpio.good() && source.good() && samples.good()
            && threshold.good() && boundary.good(), "Cannot write E-03 evidence");
}

std::vector<SourceEdge> new_pa0_edges(const Renode& renode, size_t& next)
{
    std::vector<SourceEdge> result;
    while (next < renode.capture().count) {
        const auto& event = renode.capture().events[next++];
        if (event.pin == 0) {
            result.push_back({static_cast<double>(event.timestamp_us) * 1e-6, 0.0,
                              event.timestamp_us, event.state, false});
        }
    }
    return result;
}

void write_result(const std::filesystem::path& directory, std::string_view mode,
                  uint64_t quantum_us, int64_t shift_ns, const Renode& renode,
                  const std::vector<Crossing>& crossings,
                  const std::vector<Boundary>& boundaries, bool replay)
{
    size_t late = 0, applied = 0, exact = 0;
    for (const auto& crossing : crossings) {
        late += crossing.late ? 1u : 0u;
        applied += crossing.applied ? 1u : 0u;
    }
    for (const auto& boundary : boundaries) { exact += boundary.exact_joint_boundary ? 1u : 0u; }
    const auto state = renode.status();
    std::ofstream json(directory / "result.json");
    json << "{\n"
         << "  \"status\": \"passed\",\n"
         << "  \"mode\": \"" << mode << "\",\n"
         << "  \"classification\": \"" << (replay ? "known_schedule_replay" : "sampled_approximate") << "\",\n"
         << "  \"quantum_us\": " << quantum_us << ",\n"
         << "  \"shift_ns\": " << shift_ns << ",\n"
         << "  \"final_time_us\": " << renode.now() << ",\n"
         << "  \"firmware_ticks\": " << state.ticks << ",\n"
         << "  \"exti_count\": " << state.exti_count << ",\n"
         << "  \"threshold_count\": " << crossings.size() << ",\n"
         << "  \"applied_thresholds\": " << applied << ",\n"
         << "  \"late_thresholds\": " << late << ",\n"
         << "  \"boundary_count\": " << boundaries.size() << ",\n"
         << "  \"exact_joint_boundaries\": " << exact << ",\n"
         << "  \"gpio_event_count\": " << renode.event_count() << "\n"
         << "}\n";
    require(json.good(), "Cannot write E-03 result");
}

void execute_replay(Renode& renode, Analog& analog, int64_t shift_ns,
                    const std::filesystem::path& directory)
{
    renode.prepare();
    size_t next = 0;
    for (uint64_t target = 100; target <= final_time_us; target += 100) {
        renode.advance(target);
    }
    renode.verify_boot();
    for (const auto& edge : new_pa0_edges(renode, next)) {
        analog.add_edge(edge.origin_us, edge.state, shift_ns);
    }
    require(analog.schedule().size() == 3, "Firmware GPIO schedule changed");
    analog.run_all();
    CrossingDetector detector;
    detector.scan(analog);
    require(detector.crossings().size() == 2, "Replay did not produce both Schmitt crossings");
    const std::vector<Boundary> boundaries{{
        final_time_us, renode.now(), analog.samples().back().time_s,
        std::abs(analog.samples().back().time_s - 0.010) <= 1e-12}};
    write_common(directory, renode, analog, detector.crossings(), boundaries);
    write_result(directory, "replay", 0, shift_ns, renode,
                 detector.crossings(), boundaries, true);
}

void execute_sampled(Renode& renode, Analog& analog, uint64_t quantum_us,
                     int64_t shift_ns, const std::filesystem::path& directory)
{
    require(quantum_us == 20 || quantum_us == 100 || quantum_us == 1000,
            "Unsupported predeclared quantum");
    renode.prepare();
    size_t next = 0;
    CrossingDetector detector;
    std::vector<Boundary> boundaries;
    for (uint64_t target = quantum_us; target <= final_time_us; target += quantum_us) {
        renode.advance(target);
        for (const auto& edge : new_pa0_edges(renode, next)) {
            analog.add_edge(edge.origin_us, edge.state, shift_ns);
        }
        analog.advance(static_cast<double>(target) * 1e-6);
        detector.scan(analog);
        detector.apply_ready(renode);
        const double actual = analog.samples().back().time_s;
        boundaries.push_back({target, renode.now(), actual,
                              std::abs(actual - static_cast<double>(target) * 1e-6) <= 1e-12});
    }
    renode.verify_boot();
    detector.apply_ready(renode);
    require(analog.schedule().size() == 3, "Live firmware GPIO schedule changed");
    require(detector.crossings().size() == 2, "Sampled coupling missed a Schmitt crossing");
    require(std::ranges::all_of(detector.crossings(), [](const Crossing& item) { return item.applied; }),
            "A detected threshold was never applied");
    const auto state = renode.status();
    require(state.exti_count == 2 && state.exti_input == 0 && !renode.pin(4),
            "Firmware did not acknowledge both coupled threshold transitions");
    write_common(directory, renode, analog, detector.crossings(), boundaries);
    write_result(directory, "sampled", quantum_us, shift_ns, renode,
                 detector.crossings(), boundaries, false);
}

void execute_digital(Renode& renode, uint64_t quantum_us,
                     const std::filesystem::path& directory)
{
    require(quantum_us == 20 || quantum_us == 100 || quantum_us == 1000,
            "Unsupported digital-test quantum");
    renode.prepare();
    renode.advance(500);
    renode.verify_boot();
    std::vector<uint64_t> widths{1, 5, 20, quantum_us - 1, quantum_us, quantum_us + 1};
    std::sort(widths.begin(), widths.end());
    widths.erase(std::unique(widths.begin(), widths.end()), widths.end());
    struct Pulse { uint64_t width, start, end; uint32_t before, after; };
    std::vector<Pulse> pulses;
    for (const auto width : widths) {
        const auto before = renode.status().exti_count;
        const auto start = renode.now();
        renode.input(true);
        renode.advance(start + width);
        renode.input(false);
        renode.advance(renode.now() + 100);
        pulses.push_back({width, start, start + width, before, renode.status().exti_count});
    }
    const auto same_before = renode.status().exti_count;
    const auto same_time = renode.now();
    renode.input(true);
    renode.input(false);
    renode.advance(renode.now() + 100);
    const auto same_after = renode.status().exti_count;
    const auto late_before = renode.status().exti_count;
    const uint64_t rejected_event_us = renode.now() - 1;
    const bool late_rejected = rejected_event_us < renode.now();
    require(late_rejected, "Past-event guard did not reject the event");
    renode.advance(renode.now() + 10);
    require(renode.status().exti_count == late_before, "Rejected late input reached firmware");

    std::ofstream csv(directory / "digital.csv");
    csv << "width_us,start_us,end_us,interrupts_before,interrupts_after,interrupt_delta\n";
    for (const auto& pulse : pulses) {
        csv << pulse.width << ',' << pulse.start << ',' << pulse.end << ',' << pulse.before
            << ',' << pulse.after << ',' << pulse.after - pulse.before << '\n';
    }
    std::ofstream json(directory / "result.json");
    json << "{\n  \"status\": \"passed\",\n  \"mode\": \"digital\",\n"
         << "  \"quantum_us\": " << quantum_us << ",\n"
         << "  \"pulse_count\": " << pulses.size() << ",\n"
         << "  \"same_time_us\": " << same_time << ",\n"
         << "  \"same_time_interrupt_delta\": " << same_after - same_before << ",\n"
         << "  \"late_event_us\": " << rejected_event_us << ",\n"
         << "  \"late_input_rejected\": true,\n"
         << "  \"final_time_us\": " << renode.now() << "\n}\n";
    require(csv.good() && json.good(), "Cannot write digital evidence");
}

void execute_failure(Renode& renode, const std::filesystem::path& directory)
{
    renode.prepare();
    renode.advance(500);
    renode.verify_boot();
    const int error = renode.advance_expect_failure(
        500000, directory / "accepted-request.marker");
    require(error != ERR_NO_ERROR, "Injected backend termination did not fail RunFor");
    std::ofstream json(directory / "result.json");
    json << "{\n  \"status\": \"expected_failure_observed\",\n"
         << "  \"mode\": \"failure\",\n  \"error_code\": " << error << ",\n"
         << "  \"joint_commit\": false\n}\n";
    require(json.good(), "Cannot write failure evidence");
}

void execute_recovery(Renode& renode, const std::filesystem::path& directory)
{
    renode.prepare();
    renode.advance(600);
    renode.verify_boot();
    std::ofstream json(directory / "result.json");
    json << "{\n  \"status\": \"passed\",\n  \"mode\": \"recovery\",\n"
         << "  \"fresh_backend\": true,\n  \"joint_commit_us\": 600\n}\n";
    require(json.good(), "Cannot write recovery evidence");
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 10) {
        std::fputs("Usage: e03_probe PORT MODE Q_US SHIFT_NS DLL AUDIO INIT FIXTURE OUTPUT\n", stderr);
        return 2;
    }
    try {
        const std::string mode = argv[2];
        const uint64_t quantum_us = std::stoull(argv[3]);
        const int64_t shift_ns = std::stoll(argv[4]);
        const auto dll = std::filesystem::absolute(argv[5]);
        const auto audio = std::filesystem::absolute(argv[6]);
        const auto init = std::filesystem::absolute(argv[7]);
        const auto fixture = std::filesystem::absolute(argv[8]);
        const auto directory = std::filesystem::absolute(argv[9]);
        std::filesystem::create_directories(directory);
        Renode renode(argv[1]);
        if (mode == "digital") { execute_digital(renode, quantum_us, directory); return 0; }
        if (mode == "failure") { execute_failure(renode, directory); return 0; }
        if (mode == "recovery") { execute_recovery(renode, directory); return 0; }
        require(mode == "replay" || mode == "sampled", "Unknown E-03 mode");
        simnodus::experiment::configure_initialization(init);
        simnodus::experiment::DllDirectory search(audio);
        simnodus::experiment::Library library(dll);
        Analog analog(library);
        analog.load(fixture);
        if (mode == "replay") {
            execute_replay(renode, analog, shift_ns, directory);
        } else {
            execute_sampled(renode, analog, quantum_us, shift_ns, directory);
        }
        analog.shutdown();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "E-03 host failure: %s\n", error.what());
        return 1;
    }
}
