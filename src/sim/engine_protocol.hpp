// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Messages between the editor and the engine process (SN-151, ADR 0101).
// Both ends are the same build on the same machine, so values are written in
// the machine's byte order without a schema; a version number in the Start
// message rejects a mismatched engine. A frame is a 32-bit length followed by
// the payload, whose first byte is the message type.
#pragma once

#include "mcu/mcu_backend.hpp"
#include "sim/ngspice_runtime.hpp"
#include "sim/sample_ring.hpp"
#include "sim/simulation_session.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace simnodus::sim::protocol {

inline constexpr std::uint32_t version = 4; // 2: devices (SN-203), 3: circuit nets (SN-205), 4: display texts (SN-206)
// Largest accepted frame; a transient result of many vectors fits.
inline constexpr std::uint32_t max_frame = 1u << 30;

enum class Type : std::uint8_t {
    // Editor -> engine.
    Start = 1,
    Pause,
    Resume,
    Stop,
    Speed,
    Control,
    McuReset,
    UartSend,
    Quit,
    // Engine -> editor.
    Started = 64, // reply to Start: ok, error text, status, vector names
    Update,       // new state of the run (see EngineServer::publish)
    Stopped,      // reply to Stop, after a last Update
};

class Writer {
public:
    explicit Writer(Type type) { u8(static_cast<std::uint8_t>(type)); }

    template <typename T> void raw(T value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto at = data_.size();
        data_.resize(at + sizeof(T));
        std::memcpy(data_.data() + at, &value, sizeof(T));
    }
    void u8(std::uint8_t v) { raw(v); }
    void boolean(bool v) { raw<std::uint8_t>(v ? 1 : 0); }
    void u32(std::uint32_t v) { raw(v); }
    void u64(std::uint64_t v) { raw(v); }
    void i32(std::int32_t v) { raw(v); }
    void f64(double v) { raw(v); }
    void str(const std::string& s)
    {
        u32(static_cast<std::uint32_t>(s.size()));
        data_ += s;
    }
    void size(std::size_t n) { u32(static_cast<std::uint32_t>(n)); }

    const std::string& payload() const { return data_; }

private:
    std::string data_;
};

// Reads a payload; after a short read every value is zero and ok() is false.
class Reader {
public:
    explicit Reader(const std::string& payload) : data_(payload) {}

    Type type()
    {
        return static_cast<Type>(u8());
    }
    template <typename T> T raw()
    {
        T value{};
        if (!ok_ || data_.size() - pos_ < sizeof(T)) {
            ok_ = false;
            return value;
        }
        std::memcpy(&value, data_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return value;
    }
    std::uint8_t u8() { return raw<std::uint8_t>(); }
    bool boolean() { return u8() != 0; }
    std::uint32_t u32() { return raw<std::uint32_t>(); }
    std::uint64_t u64() { return raw<std::uint64_t>(); }
    std::int32_t i32() { return raw<std::int32_t>(); }
    double f64() { return raw<double>(); }
    std::string str()
    {
        const std::uint32_t n = u32();
        if (!ok_ || data_.size() - pos_ < n) {
            ok_ = false;
            return {};
        }
        std::string s = data_.substr(pos_, n);
        pos_ += n;
        return s;
    }
    // A count of following items, checked against the bytes left (each item at least `min_bytes`).
    std::size_t size(std::size_t min_bytes = 1)
    {
        const std::uint32_t n = u32();
        if (ok_ && static_cast<std::uint64_t>(n) * min_bytes > data_.size() - pos_) {
            ok_ = false;
        }
        return ok_ ? n : 0;
    }

    bool ok() const { return ok_; }

private:
    const std::string& data_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

// The frame of a payload: its length, then the payload.
std::string frame(const std::string& payload);

// Splits a byte stream into payloads.
class FrameReader {
public:
    void append(const char* data, std::size_t size) { buffer_.append(data, size); }
    // The next complete payload; nullopt until one has arrived. A length above
    // max_frame makes the stream invalid.
    std::optional<std::string> next();
    bool invalid() const { return invalid_; }

private:
    std::string buffer_;
    std::size_t pos_ = 0;
    bool invalid_ = false;
};

// Where the engine finds what the editor has: its component library folder and
// the application folder that holds ngspice.
struct EngineSetup {
    std::filesystem::path library_dir;
    std::filesystem::path application_dir;
    std::optional<NgspiceLocation> ngspice; // an explicit ngspice (tests)
};

void write_status(Writer& w, const RunStatus& s);
RunStatus read_status(Reader& r);
void write_mcu_status(Writer& w, const mcu::McuStatus& s);
mcu::McuStatus read_mcu_status(Reader& r);
void write_pin_state(Writer& w, const mcu::PinState& s);
mcu::PinState read_pin_state(Reader& r);
void write_samples(Writer& w, const std::vector<Sample>& samples);
std::vector<Sample> read_samples(Reader& r);
void write_transient(Writer& w, const TransientResult& t);
TransientResult read_transient(Reader& r);

// Start: a run request without its library (the engine loads `setup.library_dir`
// and adds the schematic's blocks).
std::string start_message(const RunRequest& request, const EngineSetup& setup);
// Reads a Start payload after its type byte; false with `error` for a bad or mismatched message.
bool read_start(Reader& r, RunRequest& request, EngineSetup& setup, std::string& error);

} // namespace simnodus::sim::protocol
