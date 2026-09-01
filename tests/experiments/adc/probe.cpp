// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mailbox.h"

#include <windows.h>
#undef NO_ERROR
extern "C" {
#include "renode_api.h"
}

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr uint64_t AdcBase = 0x40012400u;
constexpr uint64_t AdcStatus = AdcBase + 0x00u;
constexpr uint64_t AdcControl2 = AdcBase + 0x08u;
constexpr uint64_t AdcSampleTime2 = AdcBase + 0x10u;
constexpr uint64_t AdcSequence3 = AdcBase + 0x34u;
constexpr uint64_t AdcData = AdcBase + 0x4cu;
constexpr uint32_t Eoc = 1u << 1u;
constexpr uint32_t Adon = 1u << 0u;
constexpr uint32_t ExtTrig = 1u << 20u;
constexpr uint32_t SwStart = 1u << 22u;
constexpr uint32_t ReferenceMicrovolts = 3300000u;
constexpr std::array<uint32_t, 8> ConversionDurations = {14u, 20u, 26u, 41u, 54u, 68u, 84u, 252u};

void require(bool condition, const char* message)
{
    if(!condition)
    {
        throw std::runtime_error(message);
    }
}

int consume(renode_error_t* error)
{
    if(!error)
    {
        return ERR_NO_ERROR;
    }
    const int code = error->code;
    std::cerr << "Backend diagnostic " << code << ": "
              << (error->message ? error->message : "(none)") << '\n';
    renode_free_error(error);
    return code;
}

void success(renode_error_t* error)
{
    require(consume(error) == ERR_NO_ERROR, "Unexpected backend error");
}

uint32_t expectedCode(uint32_t microvolts)
{
    const auto scaled = static_cast<uint64_t>(microvolts) * 4096u / ReferenceMicrovolts;
    return static_cast<uint32_t>(scaled > 4095u ? 4095u : scaled);
}

struct Conversion
{
    uint32_t inputMicrovolts;
    uint32_t expected;
    uint32_t actual;
    uint32_t channel;
    uint64_t beforeUs;
    uint64_t afterUs;
};

class Experiment
{
public:
    explicit Experiment(const char* port)
    {
        success(renode_connect(port, &connection));
        success(renode_get_machine(connection, "sn015", &machine));
        success(renode_get_sysbus(machine, &bus));
        success(renode_get_adc(machine, "adc1", &adc));
    }

    ~Experiment()
    {
        std::free(adc);
        std::free(bus);
        std::free(machine);
        if(connection)
        {
            consume(renode_disconnect(&connection));
        }
    }

    Experiment(const Experiment&) = delete;
    Experiment& operator=(const Experiment&) = delete;

    uint64_t timeUs() const
    {
        uint64_t result = 0;
        success(renode_get_current_time(connection, TU_MICROSECONDS, &result));
        return result;
    }

    void run(uint64_t durationUs)
    {
        const auto before = timeUs();
        success(renode_run_for(connection, TU_MICROSECONDS, durationUs));
        require(timeUs() == before + durationUs, "Renode did not advance by the requested interval");
    }

    uint32_t read32(uint64_t address) const
    {
        uint32_t result = 0;
        success(renode_sysbus_read(bus, address, AW_DOUBLE_WORD, &result, 1));
        return result;
    }

    void write32(uint64_t address, uint32_t value)
    {
        success(renode_sysbus_write(bus, address, AW_DOUBLE_WORD, &value, 1));
    }

    sn_e04_mailbox mailbox() const
    {
        sn_e04_mailbox result{};
        success(renode_sysbus_read(bus, SN_E04_MAILBOX_ADDRESS, AW_MULTI_BYTE, &result, sizeof(result)));
        require(result.fault == 0, "Firmware reported a fault");
        return result;
    }

    void setVoltage(int32_t channel, uint32_t microvolts)
    {
        success(renode_set_adc_channel_value(adc, channel, microvolts));
    }

    uint32_t voltage(int32_t channel) const
    {
        uint32_t result = 0;
        success(renode_get_adc_channel_value(adc, channel, &result));
        return result;
    }

    Conversion convert(uint32_t channel, uint32_t microvolts, uint32_t sampleSetting = 0)
    {
        setVoltage(static_cast<int32_t>(channel), microvolts);
        write32(SN_E04_MAILBOX_ADDRESS + offsetof(sn_e04_mailbox, channel), channel);
        write32(SN_E04_MAILBOX_ADDRESS + offsetof(sn_e04_mailbox, sample_setting), sampleSetting);
        ++command;
        const auto before = timeUs();
        write32(SN_E04_MAILBOX_ADDRESS + offsetof(sn_e04_mailbox, command), command);
        run(60);
        const auto result = mailbox();
        require(result.started == command && result.completed == command && result.acknowledged == command,
                "Firmware conversion command did not complete");
        require((result.status_before_read & Eoc) != 0, "Firmware did not observe EOC before reading DR");
        require((read32(AdcStatus) & Eoc) == 0, "Firmware DR read did not clear EOC");
        return {microvolts, expectedCode(microvolts), result.result, channel, before, timeUs()};
    }

    void boot()
    {
        run(100);
        require(mailbox().magic == SN_E04_BOOT_MAGIC, "E-04 firmware did not boot");
    }

    int32_t channelCount() const
    {
        int32_t result = 0;
        success(renode_get_adc_channel_count(adc, &result));
        return result;
    }

    int expectedSetFailure(int32_t channel, uint32_t value)
    {
        return consume(renode_set_adc_channel_value(adc, channel, value));
    }

private:
    renode_t* connection = nullptr;
    renode_machine_t* machine = nullptr;
    renode_bus_context_t* bus = nullptr;
    renode_adc_t* adc = nullptr;
    uint32_t command = 0;
};

void printConversion(const Conversion& item)
{
    std::cout << "{\"input_uv\":" << item.inputMicrovolts
              << ",\"expected_code\":" << item.expected
              << ",\"actual_code\":" << item.actual
              << ",\"channel\":" << item.channel
              << ",\"before_us\":" << item.beforeUs
              << ",\"after_us\":" << item.afterUs << '}';
}

template<typename T>
void printConversions(const std::vector<T>& items)
{
    std::cout << '[';
    for(std::size_t index = 0; index < items.size(); ++index)
    {
        if(index)
        {
            std::cout << ',';
        }
        printConversion(items[index]);
    }
    std::cout << ']';
}

void run(const char* port)
{
    Experiment experiment(port);
    experiment.boot();
    require(experiment.channelCount() == 16, "ADC external channel count changed");

    const std::array<uint32_t, 6> roundtripValues = {
        0u, 825000u, 1650000u, 2475000u, 3300000u, 3400000u,
    };
    for(const auto value : roundtripValues)
    {
        experiment.setVoltage(0, value);
        require(experiment.voltage(0) == value, "IADC microvolt value was quantized before conversion");
    }

    experiment.setVoltage(0, 123456u);
    const auto invalidTime = experiment.timeUs();
    const int negativeChannelCode = experiment.expectedSetFailure(-1, 999u);
    const int highChannelCode = experiment.expectedSetFailure(16, 999u);
    require(negativeChannelCode == ERR_COMMAND_FAILED && highChannelCode == ERR_COMMAND_FAILED,
            "Invalid ADC channel did not return a command error");
    require(experiment.voltage(0) == 123456u && experiment.timeUs() == invalidTime,
            "Invalid ADC channel changed state or time");

    std::vector<Conversion> staticPoints;
    for(const auto value : std::array<uint32_t, 5>{0u, 825000u, 1650000u, 2475000u, 3300000u})
    {
        staticPoints.push_back(experiment.convert(0, value));
    }

    std::vector<Conversion> boundaryPoints;
    for(const auto value : std::array<uint32_t, 4>{805u, 806u, 3299999u, 3400000u})
    {
        boundaryPoints.push_back(experiment.convert(0, value));
    }

    experiment.setVoltage(0, 825000u);
    experiment.setVoltage(1, 2475000u);
    const auto mapping0 = experiment.convert(0, experiment.voltage(0));
    const auto mapping1 = experiment.convert(1, experiment.voltage(1));

    std::vector<Conversion> ramp;
    ramp.reserve(101);
    for(uint32_t index = 0; index <= 100; ++index)
    {
        ramp.push_back(experiment.convert(0, index * 33000u));
    }

    std::cout << "{\"channel_count\":" << experiment.channelCount() << ",\"api_roundtrip_uv\":[";
    for(std::size_t index = 0; index < roundtripValues.size(); ++index)
    {
        if(index)
        {
            std::cout << ',';
        }
        std::cout << roundtripValues[index];
    }
    std::cout << "],\"invalid_channels\":{\"negative_code\":" << negativeChannelCode
              << ",\"high_code\":" << highChannelCode
              << ",\"time_unchanged\":true,\"valid_value_preserved\":true},\"static\":";
    printConversions(staticPoints);
    std::cout << ",\"boundaries\":";
    printConversions(boundaryPoints);
    std::cout << ",\"mapping\":[";
    printConversion(mapping0);
    std::cout << ',';
    printConversion(mapping1);
    std::cout << "],\"ramp\":";
    printConversions(ramp);

    std::cout << ",\"timing\":[";
    for(std::size_t setting = 0; setting < ConversionDurations.size(); ++setting)
    {
        if(setting)
        {
            std::cout << ',';
        }
        static_cast<void>(experiment.read32(AdcData));
        experiment.setVoltage(0, 1650000u);
        experiment.write32(AdcSampleTime2, static_cast<uint32_t>(setting));
        experiment.write32(AdcSequence3, 0);
        const auto started = experiment.timeUs();
        experiment.write32(AdcControl2, Adon | ExtTrig | SwStart);
        require((experiment.read32(AdcStatus) & Eoc) == 0, "EOC was set at conversion start");
        experiment.run(ConversionDurations[setting] - 1u);
        require((experiment.read32(AdcStatus) & Eoc) == 0, "EOC was set before the declared endpoint");
        experiment.run(1);
        require((experiment.read32(AdcStatus) & Eoc) != 0, "EOC was not set at the declared endpoint");
        const auto actual = experiment.read32(AdcData);
        require(actual == expectedCode(1650000u), "Direct timing conversion returned the wrong code");
        std::cout << "{\"setting\":" << setting
                  << ",\"declared_duration_us\":" << ConversionDurations[setting]
                  << ",\"started_us\":" << started
                  << ",\"completed_us\":" << experiment.timeUs()
                  << ",\"actual_code\":" << actual << '}';
    }
    std::cout << ']';

    static_cast<void>(experiment.read32(AdcData));
    experiment.setVoltage(0, 825000u);
    experiment.write32(AdcSampleTime2, 0);
    experiment.write32(AdcSequence3, 0);
    const auto snapshotStart = experiment.timeUs();
    experiment.write32(AdcControl2, Adon | ExtTrig | SwStart);
    experiment.run(1);
    experiment.setVoltage(0, 2475000u);
    const auto updateTime = experiment.timeUs();
    experiment.run(13);
    require((experiment.read32(AdcStatus) & Eoc) != 0, "Snapshot conversion did not complete");
    const auto snapshotCode = experiment.read32(AdcData);
    require(snapshotCode == expectedCode(825000u) && experiment.voltage(0) == 2475000u,
            "In-flight conversion did not retain its start sample");

    static_cast<void>(experiment.read32(AdcData));
    experiment.write32(AdcControl2, SwStart);
    const auto disabledStart = experiment.timeUs();
    experiment.run(20);
    const bool disabledEoc = (experiment.read32(AdcStatus) & Eoc) != 0;
    require(!disabledEoc, "Disabled software start produced a conversion");

    std::cout << ",\"start_snapshot\":{\"started_us\":" << snapshotStart
              << ",\"updated_us\":" << updateTime
              << ",\"completed_us\":" << updateTime + 13u
              << ",\"start_input_uv\":825000,\"updated_input_uv\":2475000,\"actual_code\":"
              << snapshotCode << "},\"disabled_start\":{\"started_us\":" << disabledStart
              << ",\"checked_us\":" << experiment.timeUs()
              << ",\"eoc\":" << (disabledEoc ? "true" : "false")
              << "},\"final_time_us\":" << experiment.timeUs() << "}\n";
}
} // namespace

int main(int argc, char** argv)
{
    if(argc != 2)
    {
        std::cerr << "Usage: e04_probe <port>\n";
        return 2;
    }
    try
    {
        run(argv[1]);
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << "E-04 probe failed: " << error.what() << '\n';
        return 1;
    }
}
