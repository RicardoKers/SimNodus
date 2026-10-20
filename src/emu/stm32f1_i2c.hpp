// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F1 I2C1 and I2C2 in master mode for the SimNodus emulator (SN-205;
// RM0008 chapter 26). The peripheral drives SCL and SDA as open-drain outputs
// (low or released) at the times its clock settings give (standard mode
// CCR / CCR, fast mode 2:1 or 16:9 duty), reads SDA when SCL is high, and
// waits while SCL stays low after it was released (a slave stretching the
// clock, or a missing pull-up resistor, reported once). Chips on the board's
// nets answer within the bus cycle (ADR 0102).
// Simulated: START and repeated START, 7-bit addressing, acknowledge and
// AF, transmission with TXE and BTF, reception with RXNE, BTF, ACK and POS as
// STM32CubeF1's HAL sequences use them, STOP, SB, ADDR, MSL, BUSY, TRA,
// interrupts (event and error) and DMA requests (I2C1 TX/RX on DMA1 channels
// 6/7, I2C2 on 4/5), the I2C1 remap and SWRST. Not simulated (reported once):
// slave mode, 10-bit addressing, SMBus, PEC, DMA's LAST bit, arbitration.
#pragma once

#include "bus.hpp"
#include "timed_device.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace simnodus::emu {

class I2cHost {
public:
    virtual ~I2cHost() = default;
    virtual std::uint64_t i2c_now_cycles() const = 0;
    virtual std::uint32_t hclk_per_pclk1() const = 0;
    virtual std::uint32_t afio_mapr() const = 0;
    virtual void i2c_irq(int irq, bool level) = 0;
    // An output of the I2C changed at `cycle` (true: released).
    virtual void i2c_output(int port, int pin, int source, bool level, std::uint64_t cycle) = 0;
    virtual bool pin_level(int port, int pin) const = 0;
    virtual bool i2c_dma(int channel) = 0;
    virtual void i2c_schedule_changed() = 0;
    virtual void i2c_diagnostic(const std::string& text) = 0;
};

class Stm32F1I2c : public Device, public TimedDevice {
public:
    // number: 1 or 2 (both on APB1).
    Stm32F1I2c(I2cHost& host, int number);

    const char* name() const override { return number_ == 1 ? "I2C1" : "I2C2"; }
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    void reset() override;

    std::uint64_t next_event() const override;
    void on_time(std::uint64_t cycle) override;
    // A pin's level changed (SCL released by a slave that stretched it).
    void pin_changed(int port, int pin, bool level);
    void remap_changed();
    void dma_enabled() { service(); }

private:
    enum class Phase {
        Idle,       // no transfer
        Start1,     // SDA low with SCL high
        Start2,     // then SCL low: SB
        Restart1,   // from SCL low: release SDA
        Restart2,   // release SCL (then Start1)
        Stalled,    // SCL held low, waiting for the firmware (SB, ADDR, BTF, AF)
        BitLow,     // SCL low: put the bit on SDA
        BitRise,    // release SCL, sample SDA once it is high
        BitFall,    // SCL low again
        Stop1,      // SDA low with SCL low
        Stop2,      // release SCL
        Stop3,      // release SDA with SCL high: the bus is free
    };
    enum class Stall { None, Sb, Addr, TxEmpty, Btf, Af };

    std::pair<int, int> scl() const;
    std::pair<int, int> sda() const;
    void drive(bool scl_side, bool released, std::uint64_t cycle);
    std::uint64_t low_cycles() const;
    std::uint64_t high_cycles() const;
    void schedule(Phase phase, std::uint64_t at);
    void enter(Phase phase, std::uint64_t cycle); // that phase's action now
    void step(std::uint64_t cycle);
    void sample();
    void stall(Stall why);
    void begin_byte(std::uint8_t value, bool transmit, std::uint64_t cycle);
    void byte_done(std::uint64_t cycle);
    void after_byte(std::uint64_t cycle); // what follows a byte when the firmware does not hold the bus
    void request_stop(std::uint64_t cycle);
    void request_start(std::uint64_t cycle);
    void clear_addr(std::uint64_t cycle);
    void read_dr(std::uint64_t cycle);
    void write_dr(std::uint32_t value, std::uint64_t cycle);
    void release_lines(std::uint64_t cycle);
    void service();

    I2cHost& host_;
    int number_;
    int irq_ev_, irq_er_;
    int dma_tx_, dma_rx_;
    // Registers.
    std::uint32_t cr1_ = 0, cr2_ = 0, oar1_ = 0, oar2_ = 0, ccr_ = 0, trise_ = 2;
    bool sb_ = false, addr_ = false, btf_ = false, rxne_ = false, txe_ = false, af_ = false, ovr_ = false;
    bool msl_ = false, busy_ = false, tra_ = false;
    bool sr1_read_ = false; // SR1 was read: the next SR2 read clears ADDR
    std::uint8_t dr_ = 0;
    bool dr_full_ = false; // transmitter: a byte waits in DR
    // Bus state.
    Phase phase_ = Phase::Idle;
    std::uint64_t at_ = no_event;
    Stall stall_ = Stall::None;
    bool waiting_scl_ = false;     // SCL released but still low
    std::uint64_t wait_since_ = 0;
    bool stop_pending_ = false, start_pending_ = false;
    bool transmit_ = true;         // current byte: master sends it
    bool address_byte_ = false;
    std::uint8_t shift_ = 0;
    int bit_ = 0;                  // 0..8 (8: acknowledge)
    bool ack_ = false;             // acknowledge seen (transmit) or to give (receive)
    bool pos_ack_ = false;         // POS: the acknowledge latched when this byte began
    bool shift_full_ = false;      // receiver: a byte waits in the shift register (BTF)
    std::pair<int, int> driven_scl_{-1, -1}, driven_sda_{-1, -1};
    bool in_service_ = false;
    bool warned_slave_ = false, warned_smbus_ = false, warned_stuck_ = false, warned_last_ = false;
};

} // namespace simnodus::emu
