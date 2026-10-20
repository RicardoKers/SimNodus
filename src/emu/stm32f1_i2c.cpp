// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "stm32f1_i2c.hpp"

#include <algorithm>

namespace simnodus::emu {

namespace {

constexpr std::uint32_t CR1_PE = 1u << 0, CR1_SMBUS = 1u << 1, CR1_ENPEC = 1u << 5, CR1_START = 1u << 8;
constexpr std::uint32_t CR1_STOP = 1u << 9, CR1_ACK = 1u << 10, CR1_POS = 1u << 11, CR1_SWRST = 1u << 15;
constexpr std::uint32_t CR2_ITERREN = 1u << 8, CR2_ITEVTEN = 1u << 9, CR2_ITBUFEN = 1u << 10;
constexpr std::uint32_t CR2_DMAEN = 1u << 11, CR2_LAST = 1u << 12;

} // namespace

Stm32F1I2c::Stm32F1I2c(I2cHost& host, int number)
    : host_(host), number_(number), irq_ev_(number == 1 ? 31 : 33), irq_er_(number == 1 ? 32 : 34),
      dma_tx_(number == 1 ? 6 : 4), dma_rx_(number == 1 ? 7 : 5)
{
}

void Stm32F1I2c::reset()
{
    cr1_ = cr2_ = oar1_ = oar2_ = ccr_ = 0;
    trise_ = 2;
    sb_ = addr_ = btf_ = rxne_ = txe_ = af_ = ovr_ = false;
    msl_ = busy_ = tra_ = false;
    sr1_read_ = false;
    dr_ = 0;
    dr_full_ = false;
    phase_ = Phase::Idle;
    at_ = no_event;
    stall_ = Stall::None;
    waiting_scl_ = false;
    stop_pending_ = start_pending_ = false;
    shift_full_ = false;
    driven_scl_ = driven_sda_ = {-1, -1}; // the machine reset released the pins
}

std::pair<int, int> Stm32F1I2c::scl() const
{
    if (number_ == 2) {
        return {1, 10};
    }
    return (host_.afio_mapr() & 2u) ? std::pair{1, 8} : std::pair{1, 6}; // I2C1_REMAP
}

std::pair<int, int> Stm32F1I2c::sda() const
{
    if (number_ == 2) {
        return {1, 11};
    }
    return (host_.afio_mapr() & 2u) ? std::pair{1, 9} : std::pair{1, 7};
}

void Stm32F1I2c::drive(bool scl_side, bool released, std::uint64_t cycle)
{
    const auto pin = scl_side ? scl() : sda();
    auto& driven = scl_side ? driven_scl_ : driven_sda_;
    const int source = 400 + 4 * number_ + (scl_side ? 0 : 1);
    if (driven != pin && driven.first >= 0) {
        host_.i2c_output(driven.first, driven.second, source, false, cycle);
    }
    driven = pin;
    host_.i2c_output(pin.first, pin.second, source, released, cycle);
}

void Stm32F1I2c::release_lines(std::uint64_t cycle)
{
    drive(true, true, cycle);
    drive(false, true, cycle);
}

void Stm32F1I2c::remap_changed()
{
    if (cr1_ & CR1_PE) {
        release_lines(host_.i2c_now_cycles());
    }
}

std::uint64_t Stm32F1I2c::low_cycles() const
{
    const std::uint64_t pclk = host_.hclk_per_pclk1();
    const std::uint64_t ccr = std::max<std::uint32_t>(ccr_ & 0xFFFu, 1);
    if (!(ccr_ & (1u << 15))) {
        return ccr * pclk; // standard mode: high = low = CCR
    }
    return ((ccr_ & (1u << 14)) ? 16 : 2) * ccr * pclk; // fast mode: 16:9 or 2:1
}

std::uint64_t Stm32F1I2c::high_cycles() const
{
    const std::uint64_t pclk = host_.hclk_per_pclk1();
    const std::uint64_t ccr = std::max<std::uint32_t>(ccr_ & 0xFFFu, 1);
    if (!(ccr_ & (1u << 15))) {
        return ccr * pclk;
    }
    return ((ccr_ & (1u << 14)) ? 9 : 1) * ccr * pclk;
}

void Stm32F1I2c::schedule(Phase phase, std::uint64_t at)
{
    phase_ = phase;
    at_ = at;
    host_.i2c_schedule_changed();
}

void Stm32F1I2c::enter(Phase phase, std::uint64_t cycle)
{
    phase_ = phase;
    at_ = no_event;
    step(cycle);
}

void Stm32F1I2c::stall(Stall why)
{
    phase_ = Phase::Stalled;
    at_ = no_event;
    stall_ = why;
    service();
}

void Stm32F1I2c::sample()
{
    const auto [port, pin] = sda();
    const bool level = host_.pin_level(port, pin);
    if (bit_ < 8) {
        if (!transmit_) {
            shift_ = static_cast<std::uint8_t>((shift_ << 1) | (level ? 1u : 0u));
        }
    } else if (transmit_) {
        ack_ = !level; // the slave pulls SDA low to acknowledge
    }
}

void Stm32F1I2c::step(std::uint64_t cycle)
{
    switch (phase_) {
    case Phase::Start1:
        drive(false, false, cycle); // SDA falls while SCL is high
        schedule(Phase::Start2, cycle + high_cycles());
        break;
    case Phase::Start2:
        drive(true, false, cycle);
        sb_ = msl_ = busy_ = true;
        cr1_ &= ~CR1_START;
        start_pending_ = false;
        stall(Stall::Sb);
        break;
    case Phase::Restart1:
        drive(false, true, cycle);
        schedule(Phase::Restart2, cycle + low_cycles() / 2);
        break;
    case Phase::Restart2:
    case Phase::BitRise:
    case Phase::Stop2: {
        drive(true, true, cycle);
        const auto [port, pin] = scl();
        if (!host_.pin_level(port, pin)) {
            // Held low: a slave stretches the clock, or nothing pulls the line up.
            waiting_scl_ = true;
            wait_since_ = cycle;
            at_ = cycle + 200 * low_cycles();
            host_.i2c_schedule_changed();
            break;
        }
        if (phase_ == Phase::BitRise) {
            sample();
            schedule(Phase::BitFall, cycle + high_cycles());
        } else {
            schedule(phase_ == Phase::Restart2 ? Phase::Start1 : Phase::Stop3, cycle + high_cycles());
        }
        break;
    }
    case Phase::BitLow: {
        bool released = true;
        if (transmit_ && bit_ < 8) {
            released = ((shift_ >> (7 - bit_)) & 1u) != 0;
        } else if (!transmit_ && bit_ == 8) {
            ack_ = (cr1_ & CR1_POS) ? pos_ack_ : (cr1_ & CR1_ACK) != 0;
            released = !ack_;
        }
        drive(false, released, cycle);
        schedule(Phase::BitRise, cycle + low_cycles());
        break;
    }
    case Phase::BitFall:
        drive(true, false, cycle);
        if (++bit_ <= 8) {
            enter(Phase::BitLow, cycle);
        } else {
            byte_done(cycle);
        }
        break;
    case Phase::Stop1:
        drive(false, false, cycle); // SDA low while SCL is low
        schedule(Phase::Stop2, cycle + low_cycles() / 2);
        break;
    case Phase::Stop3:
        drive(false, true, cycle); // SDA rises while SCL is high: STOP
        busy_ = msl_ = tra_ = false;
        cr1_ &= ~CR1_STOP;
        stop_pending_ = false;
        phase_ = Phase::Idle;
        at_ = no_event;
        stall_ = Stall::None;
        service();
        if (start_pending_) {
            schedule(Phase::Start1, cycle + low_cycles()); // bus free time, then START
        }
        break;
    case Phase::Idle:
    case Phase::Stalled:
        break;
    }
}

void Stm32F1I2c::begin_byte(std::uint8_t value, bool transmit, std::uint64_t cycle)
{
    transmit_ = transmit;
    shift_ = transmit ? value : 0;
    bit_ = 0;
    pos_ack_ = (cr1_ & CR1_ACK) != 0;
    stall_ = Stall::None;
    enter(Phase::BitLow, cycle);
}

void Stm32F1I2c::byte_done(std::uint64_t cycle)
{
    if (address_byte_) {
        address_byte_ = false;
        if (ack_) {
            addr_ = true;
            stall(Stall::Addr);
        } else {
            af_ = true;
            stall(Stall::Af);
        }
    } else if (transmit_) {
        if (!ack_) {
            af_ = true;
            stall(Stall::Af);
        } else if (stop_pending_ || start_pending_) {
            after_byte(cycle);
        } else if (dr_full_) {
            dr_full_ = false;
            txe_ = true;
            begin_byte(dr_, true, cycle);
            service();
        } else {
            btf_ = true;
            stall(Stall::Btf);
        }
    } else if (!rxne_) {
        dr_ = shift_;
        rxne_ = true;
        after_byte(cycle);
        service();
    } else {
        shift_full_ = true; // DR still holds the previous byte: hold the clock
        btf_ = true;
        stall(Stall::Btf);
    }
    if (phase_ == Phase::Stalled && stop_pending_) {
        request_stop(cycle);
    }
}

void Stm32F1I2c::after_byte(std::uint64_t cycle)
{
    if (stop_pending_) {
        enter(Phase::Stop1, cycle);
    } else if (start_pending_) {
        enter(Phase::Restart1, cycle);
    } else if (!transmit_) {
        begin_byte(0, false, cycle); // the receiver clocks in the next byte
    }
}

void Stm32F1I2c::request_stop(std::uint64_t cycle)
{
    if (phase_ == Phase::Stalled) {
        stall_ = Stall::None;
        btf_ = btf_ && shift_full_; // a receiver's waiting byte keeps BTF until DR is read
        enter(Phase::Stop1, cycle);
    } else if (phase_ == Phase::Idle) {
        cr1_ &= ~CR1_STOP; // nothing to stop
    } else {
        stop_pending_ = true;
    }
}

void Stm32F1I2c::request_start(std::uint64_t cycle)
{
    if (phase_ == Phase::Idle) {
        enter(Phase::Start1, cycle);
    } else if (phase_ == Phase::Stalled) {
        stall_ = Stall::None;
        btf_ = btf_ && shift_full_;
        enter(Phase::Restart1, cycle);
    } else {
        start_pending_ = true;
    }
}

void Stm32F1I2c::clear_addr(std::uint64_t cycle)
{
    addr_ = false;
    if (stall_ != Stall::Addr) {
        return; // a STOP already ended the transfer
    }
    if (tra_) {
        txe_ = true;
        stall(Stall::TxEmpty); // SCL stays low until DR is written
    } else {
        begin_byte(0, false, cycle);
    }
}

void Stm32F1I2c::write_dr(std::uint32_t value, std::uint64_t cycle)
{
    dr_ = static_cast<std::uint8_t>(value);
    if (stall_ == Stall::Sb) {
        sb_ = false;
        tra_ = (value & 1u) == 0;
        address_byte_ = true;
        begin_byte(dr_, true, cycle);
    } else if (stall_ == Stall::TxEmpty || (stall_ == Stall::Btf && tra_)) {
        btf_ = false;
        txe_ = true; // DR goes to the shift register at once
        begin_byte(dr_, true, cycle);
    } else if (tra_ && msl_) {
        dr_full_ = true;
        txe_ = false;
    }
    service();
}

void Stm32F1I2c::read_dr(std::uint64_t cycle)
{
    if (shift_full_) {
        dr_ = shift_; // the waiting byte moves on; RXNE stays set
        shift_full_ = false;
        btf_ = false;
        if (phase_ == Phase::Stalled) {
            stall_ = Stall::None;
            after_byte(cycle); // the clock runs again (unless a STOP or START is already under way)
        }
    } else {
        rxne_ = false;
    }
    service();
}

bool Stm32F1I2c::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    const std::uint64_t now = host_.i2c_now_cycles();
    std::uint32_t reg = 0;
    switch (offset & ~3u) {
    case 0x00: reg = cr1_; break;
    case 0x04: reg = cr2_; break;
    case 0x08: reg = oar1_; break;
    case 0x0C: reg = oar2_; break;
    case 0x10:
        reg = dr_;
        read_dr(now);
        break;
    case 0x14:
        reg = (sb_ ? 1u : 0u) | (addr_ ? 2u : 0u) | (btf_ ? 4u : 0u) | (rxne_ ? 0x40u : 0u) | (txe_ ? 0x80u : 0u)
            | (af_ ? 0x400u : 0u) | (ovr_ ? 0x800u : 0u);
        sr1_read_ = true;
        break;
    case 0x18:
        reg = (msl_ ? 1u : 0u) | (busy_ ? 2u : 0u) | (tra_ ? 4u : 0u);
        if (sr1_read_ && addr_) {
            clear_addr(now);
        }
        sr1_read_ = false;
        break;
    case 0x1C: reg = ccr_; break;
    case 0x20: reg = trise_; break;
    default: break;
    }
    value = (reg >> (8 * (offset & 3u))) & (size == 1 ? 0xFFu : 0xFFFFu);
    return true;
}

bool Stm32F1I2c::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    if ((offset & 3u) != 0) {
        return true;
    }
    const std::uint64_t now = host_.i2c_now_cycles();
    value &= size == 1 ? 0xFFu : 0xFFFFu;
    switch (offset) {
    case 0x00: {
        const std::uint32_t old = cr1_;
        if (value & CR1_SWRST) {
            reset();
            cr1_ = CR1_SWRST;
            break;
        }
        cr1_ = value;
        if ((cr1_ & (CR1_SMBUS | CR1_ENPEC)) && !warned_smbus_) {
            warned_smbus_ = true;
            host_.i2c_diagnostic(std::string(name()) + " in SMBus or PEC mode, which SimNodus does not simulate yet.");
        }
        if (!(cr1_ & CR1_PE)) {
            if (old & CR1_PE) {
                release_lines(now);
            }
            sb_ = addr_ = btf_ = rxne_ = txe_ = af_ = false;
            msl_ = busy_ = tra_ = false;
            phase_ = Phase::Idle;
            at_ = no_event;
            stall_ = Stall::None;
            stop_pending_ = start_pending_ = shift_full_ = dr_full_ = false;
            cr1_ &= ~(CR1_START | CR1_STOP);
            service();
            break;
        }
        if (!(old & CR1_PE)) {
            release_lines(now); // both lines idle high
        }
        if ((cr1_ & CR1_START) && !(old & CR1_START)) {
            request_start(now);
        }
        if ((cr1_ & CR1_STOP) && !(old & CR1_STOP)) {
            request_stop(now);
        }
        break;
    }
    case 0x04:
        cr2_ = value & 0x1F3Fu;
        if ((cr2_ & CR2_LAST) && !warned_last_) {
            warned_last_ = true;
            host_.i2c_diagnostic(std::string(name())
                + ": the LAST bit (NACK at the end of a DMA reception) is not simulated yet.");
        }
        service();
        break;
    case 0x08:
        oar1_ = value;
        break;
    case 0x0C:
        oar2_ = value;
        break;
    case 0x10:
        write_dr(value, now);
        break;
    case 0x14:
        // rc_w0 flags: AF, OVR (and the error flags SimNodus does not raise).
        af_ = af_ && (value & 0x400u);
        ovr_ = ovr_ && (value & 0x800u);
        service();
        break;
    case 0x1C:
        ccr_ = value & 0xCFFFu;
        break;
    case 0x20:
        trise_ = value & 0x3Fu;
        break;
    default: break;
    }
    return true;
}

std::uint64_t Stm32F1I2c::next_event() const
{
    return at_;
}

void Stm32F1I2c::on_time(std::uint64_t cycle)
{
    while (at_ <= cycle) {
        const std::uint64_t at = at_;
        at_ = no_event;
        if (waiting_scl_) {
            if (!warned_stuck_) {
                warned_stuck_ = true;
                const auto [port, pin] = scl();
                host_.i2c_diagnostic(std::string(name()) + ": SCL (P" + static_cast<char>('A' + port)
                    + std::to_string(pin) + ") stays low after it was released; is there a pull-up resistor on "
                    "SCL and SDA, and are the pins configured as alternate-function open-drain?");
            }
            continue;
        }
        step(at);
    }
}

void Stm32F1I2c::pin_changed(int port, int pin, bool level)
{
    if (!waiting_scl_ || !level || std::pair{port, pin} != scl()) {
        return;
    }
    waiting_scl_ = false;
    const std::uint64_t now = host_.i2c_now_cycles();
    if (phase_ == Phase::BitRise) {
        sample();
        schedule(Phase::BitFall, now + high_cycles());
    } else {
        schedule(phase_ == Phase::Restart2 ? Phase::Start1 : Phase::Stop3, now + high_cycles());
    }
}

void Stm32F1I2c::service()
{
    if (!in_service_ && (cr2_ & CR2_DMAEN)) {
        in_service_ = true;
        while (rxne_ && host_.i2c_dma(dma_rx_)) {
        }
        while (txe_ && tra_ && msl_ && !dr_full_ && !addr_ && host_.i2c_dma(dma_tx_)) {
        }
        in_service_ = false;
    }
    const bool events = (cr2_ & CR2_ITEVTEN)
        && (sb_ || addr_ || btf_ || ((cr2_ & CR2_ITBUFEN) && ((txe_ && tra_ && msl_) || rxne_)));
    const bool errors = (cr2_ & CR2_ITERREN) && (af_ || ovr_);
    host_.i2c_irq(irq_ev_, events);
    host_.i2c_irq(irq_er_, errors);
}

} // namespace simnodus::emu
