// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include <windows.h>
#undef NO_ERROR
extern "C" {
#include "renode_api.h"
}

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message)
{
    if(!condition)
    {
        throw std::runtime_error(message);
    }
}

void success(renode_error_t* error)
{
    if(!error)
    {
        return;
    }
    const std::string message = error->message ? error->message : "Unknown backend error";
    const int code = error->code;
    renode_free_error(error);
    throw std::runtime_error("Backend error " + std::to_string(code) + ": " + message);
}

uint64_t number(const char* text)
{
    char* end = nullptr;
    const auto result = std::strtoull(text, &end, 0);
    require(end && *end == '\0', "Invalid unsigned integer argument");
    return result;
}

class Session
{
public:
    explicit Session(const char* port)
    {
        success(renode_connect(port, &connection_));
        success(renode_get_machine(connection_, "sn016", &machine_));
        success(renode_get_sysbus(machine_, &bus_));
    }

    ~Session()
    {
        std::free(adc_);
        std::free(bus_);
        std::free(machine_);
        if(connection_)
        {
            auto* error = renode_disconnect(&connection_);
            if(error)
            {
                renode_free_error(error);
            }
        }
    }

    uint64_t time() const
    {
        uint64_t result = 0;
        success(renode_get_current_time(connection_, TU_MICROSECONDS, &result));
        return result;
    }

    void run(uint64_t microseconds)
    {
        success(renode_run_for(connection_, TU_MICROSECONDS, microseconds));
    }

    uint32_t read(uint64_t address) const
    {
        uint32_t result = 0;
        success(renode_sysbus_read(bus_, address, AW_DOUBLE_WORD, &result, 1));
        return result;
    }

    void write(uint64_t address, uint32_t value)
    {
        success(renode_sysbus_write(bus_, address, AW_DOUBLE_WORD, &value, 1));
    }

    void setAdc(int32_t channel, uint32_t microvolts)
    {
        if(!adc_)
        {
            success(renode_get_adc(machine_, "adc1", &adc_));
        }
        success(renode_set_adc_channel_value(adc_, channel, microvolts));
    }

private:
    renode_t* connection_ = nullptr;
    renode_machine_t* machine_ = nullptr;
    renode_bus_context_t* bus_ = nullptr;
    renode_adc_t* adc_ = nullptr;
};

void execute(int argc, char** argv)
{
    require(argc >= 3, "Usage: e05_control <port> <time|run|read32|write32|adc> [arguments]");
    Session session(argv[1]);
    const std::string command = argv[2];
    const auto before = session.time();
    if(command == "time")
    {
        require(argc == 3, "time takes no argument");
    }
    else if(command == "run")
    {
        require(argc == 4, "run requires a duration in microseconds");
        session.run(number(argv[3]));
    }
    else if(command == "read32")
    {
        require(argc == 4, "read32 requires an address");
        const auto value = session.read(number(argv[3]));
        std::cout << "{\"command\":\"read32\",\"before_us\":" << before
                  << ",\"after_us\":" << session.time() << ",\"value\":" << value << "}\n";
        return;
    }
    else if(command == "write32")
    {
        require(argc == 5, "write32 requires an address and value");
        const auto value = number(argv[4]);
        require(value <= std::numeric_limits<uint32_t>::max(), "write32 value is too large");
        session.write(number(argv[3]), static_cast<uint32_t>(value));
    }
    else if(command == "adc")
    {
        require(argc == 5, "adc requires a channel and integer microvolts");
        const auto channel = number(argv[3]);
        const auto value = number(argv[4]);
        require(channel <= std::numeric_limits<int32_t>::max(), "ADC channel is too large");
        require(value <= std::numeric_limits<uint32_t>::max(), "ADC value is too large");
        session.setAdc(static_cast<int32_t>(channel), static_cast<uint32_t>(value));
    }
    else
    {
        throw std::runtime_error("Unknown control command");
    }
    std::cout << "{\"command\":\"" << command << "\",\"before_us\":" << before
              << ",\"after_us\":" << session.time() << "}\n";
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        execute(argc, argv);
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << "E-05 control failed: " << error.what() << '\n';
        return 1;
    }
}
