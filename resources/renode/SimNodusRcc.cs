//
// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F1 reset and clock control (RCC) for SimNodus (SN-137). Renode 1.16.1
// has no STM32F1 RCC, and a plain register store never sets HSERDY/PLLRDY or
// SWS, so STM32CubeIDE HAL projects hang in SystemClock_Config(). This model:
//   - sets each ready flag as soon as its oscillator or the PLL is switched on
//     (HSI, HSE, PLL, LSE, LSI; no start-up delay);
//   - switches SWS to the SW request when the selected source is ready;
//   - computes SYSCLK and HCLK from the selected source, PLLSRC, PLLXTPRE,
//     PLLMUL and HPRE (HSE = the board crystal, 8 MHz on the Blue Pill), and
//     makes the SysTick count at HCLK and the CPU run HCLK/1 MHz MIPS
//     (one instruction per cycle; flash wait states are not modeled; at
//     least 1 MIPS);
//   - keeps the reset, enable, backup domain and reset-flag registers as
//     plain registers with their documented reset values.
// Not modeled: start-up times, clock security system, RCC interrupts, MCO,
// peripheral clock gating (peripherals run whether enabled or not) and
// bus prescalers' effect on peripherals (UART bytes move instantly anyway).
// Renode compiles this file at start-up (include), like SimNodusGpio.cs.
using System;

using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.CPU;
using Antmicro.Renode.Peripherals.IRQControllers;

namespace Antmicro.Renode.Peripherals.Miscellaneous
{
    public class SimNodusSTM32F1RCC : BasicDoubleWordPeripheral, IKnownSize
    {
        public SimNodusSTM32F1RCC(IMachine machine, NVIC nvic, BaseCPU cpu, ulong hseFrequency = 8000000)
            : base(machine)
        {
            this.nvic = nvic;
            this.cpu = cpu;
            this.hseFrequency = hseFrequency;
            DefineRegisters();
            Reset();
        }

        public override void Reset()
        {
            base.Reset();
            systemClockSource = ClockSource.Hsi;
            readyFlags = 0;
            previousReady = 4; // HSI runs from reset
            appliedHclk = 0;   // reapply: the CPU and SysTick may still run a firmware's clock
            UpdateClocks();
        }

        public long Size => 0x400;

        // Frequencies in Hz, readable from the monitor (sysbus.rcc HclkFrequency).
        public ulong SystemClockFrequency => SourceFrequency(systemClockSource);

        public ulong HclkFrequency => SystemClockFrequency >> AhbShift((uint)ahbPrescaler.Value);

        private void DefineRegisters()
        {
            Registers.Control.Define(this, 0x83, name: "RCC_CR")
                .WithFlag(0, out hsiOn, name: "HSION", changeCallback: (_, __) => UpdateReady())
                .WithFlag(1, FieldMode.Read, name: "HSIRDY", valueProviderCallback: _ => hsiOn.Value)
                .WithReservedBits(2, 1)
                .WithValueField(3, 5, name: "HSITRIM")
                .WithValueField(8, 8, FieldMode.Read, name: "HSICAL", valueProviderCallback: _ => 0)
                .WithFlag(16, out hseOn, name: "HSEON", changeCallback: (_, __) => UpdateReady())
                .WithFlag(17, FieldMode.Read, name: "HSERDY", valueProviderCallback: _ => hseOn.Value)
                .WithFlag(18, name: "HSEBYP")
                .WithFlag(19, name: "CSSON")
                .WithReservedBits(20, 4)
                .WithFlag(24, out pllOn, name: "PLLON", changeCallback: (_, __) => UpdateReady())
                .WithFlag(25, FieldMode.Read, name: "PLLRDY", valueProviderCallback: _ => pllOn.Value)
                .WithReservedBits(26, 6);

            Registers.Configuration.Define(this, name: "RCC_CFGR")
                .WithValueField(0, 2, out systemClockSwitch, name: "SW", changeCallback: (_, __) => SwitchSystemClock())
                .WithValueField(2, 2, FieldMode.Read, name: "SWS", valueProviderCallback: _ => (ulong)systemClockSource)
                .WithValueField(4, 4, out ahbPrescaler, name: "HPRE", changeCallback: (_, __) => UpdateClocks())
                .WithValueField(8, 3, name: "PPRE1")
                .WithValueField(11, 3, name: "PPRE2")
                .WithValueField(14, 2, name: "ADCPRE")
                .WithFlag(16, out pllSourceHse, name: "PLLSRC", changeCallback: (_, __) => UpdateClocks())
                .WithFlag(17, out hseDividedForPll, name: "PLLXTPRE", changeCallback: (_, __) => UpdateClocks())
                .WithValueField(18, 4, out pllMultiplier, name: "PLLMUL", changeCallback: (_, __) => UpdateClocks())
                .WithFlag(22, name: "USBPRE")
                .WithReservedBits(23, 1)
                .WithValueField(24, 3, name: "MCO")
                .WithReservedBits(27, 5);

            // Ready interrupt flags are set when a source becomes ready and cleared
            // by writing the matching clear bit; the RCC interrupt itself is not raised.
            Registers.ClockInterrupt.Define(this, name: "RCC_CIR")
                .WithValueField(0, 5, FieldMode.Read, name: "RDYF", valueProviderCallback: _ => readyFlags)
                .WithReservedBits(5, 2)
                .WithFlag(7, FieldMode.Read, name: "CSSF", valueProviderCallback: _ => false)
                .WithValueField(8, 5, name: "RDYIE")
                .WithReservedBits(13, 3)
                .WithValueField(16, 5, FieldMode.Write, name: "RDYC", writeCallback: (_, value) => readyFlags &= ~(uint)value)
                .WithReservedBits(21, 2)
                .WithFlag(23, FieldMode.Write, name: "CSSC")
                .WithReservedBits(24, 8);

            Registers.Apb2Reset.Define(this, name: "RCC_APB2RSTR").WithValueField(0, 32, name: "APB2RSTR");
            Registers.Apb1Reset.Define(this, name: "RCC_APB1RSTR").WithValueField(0, 32, name: "APB1RSTR");
            Registers.AhbEnable.Define(this, 0x14, name: "RCC_AHBENR").WithValueField(0, 32, name: "AHBENR");
            Registers.Apb2Enable.Define(this, name: "RCC_APB2ENR").WithValueField(0, 32, name: "APB2ENR");
            Registers.Apb1Enable.Define(this, name: "RCC_APB1ENR").WithValueField(0, 32, name: "APB1ENR");

            Registers.BackupDomainControl.Define(this, name: "RCC_BDCR")
                .WithFlag(0, out lseOn, name: "LSEON", changeCallback: (_, __) => UpdateReady())
                .WithFlag(1, FieldMode.Read, name: "LSERDY", valueProviderCallback: _ => lseOn.Value)
                .WithFlag(2, name: "LSEBYP")
                .WithReservedBits(3, 5)
                .WithValueField(8, 2, name: "RTCSEL")
                .WithReservedBits(10, 5)
                .WithFlag(15, name: "RTCEN")
                .WithFlag(16, name: "BDRST")
                .WithReservedBits(17, 15);

            // Reset flags: power-on and pin reset after start-up; RMVF clears them.
            Registers.ControlStatus.Define(this, 0x0C000000, name: "RCC_CSR")
                .WithFlag(0, out lsiOn, name: "LSION", changeCallback: (_, __) => UpdateReady())
                .WithFlag(1, FieldMode.Read, name: "LSIRDY", valueProviderCallback: _ => lsiOn.Value)
                .WithReservedBits(2, 22)
                .WithFlag(24, FieldMode.Write, name: "RMVF", writeCallback: (_, value) => { if(value) resetFlags.Value = 0; })
                .WithReservedBits(25, 1)
                .WithValueField(26, 6, out resetFlags, FieldMode.Read, name: "RSTF");
        }

        // Sets the CIR ready flags of sources that just came on, then follows
        // a source that was switched off while it drove the system clock.
        private void UpdateReady()
        {
            var ready = (lsiOn.Value ? 1u : 0) | (lseOn.Value ? 2u : 0) | (hsiOn.Value ? 4u : 0)
                | (hseOn.Value ? 8u : 0) | (pllOn.Value ? 16u : 0);
            readyFlags |= ready & ~previousReady;
            previousReady = ready;
            if(!IsReady(systemClockSource))
            {
                // On the chip HSI takes over only after a clock failure; switching
                // off the running source is refused. Keep the simplest safe choice.
                this.Log(LogLevel.Warning, "System clock source {0} switched off; using HSI", systemClockSource);
                systemClockSource = ClockSource.Hsi;
            }
            SwitchSystemClock();
        }

        private void SwitchSystemClock()
        {
            var requested = (ClockSource)systemClockSwitch.Value;
            if(requested == ClockSource.Invalid || !IsReady(requested))
            {
                UpdateClocks();
                return;
            }
            systemClockSource = requested;
            UpdateClocks();
        }

        private bool IsReady(ClockSource source)
        {
            switch(source)
            {
            case ClockSource.Hsi:
                return hsiOn.Value;
            case ClockSource.Hse:
                return hseOn.Value;
            case ClockSource.Pll:
                return pllOn.Value;
            default:
                return false;
            }
        }

        private ulong SourceFrequency(ClockSource source)
        {
            switch(source)
            {
            case ClockSource.Hse:
                return hseFrequency;
            case ClockSource.Pll:
                var input = pllSourceHse.Value ? hseFrequency / (hseDividedForPll.Value ? 2UL : 1UL) : HsiFrequency / 2;
                return input * Math.Min(pllMultiplier.Value + 2, 16UL);
            default:
                return HsiFrequency;
            }
        }

        private static int AhbShift(uint hpre)
        {
            // 0xxx: /1; 1000..1011: /2../16; 1100..1111: /64../512 (no /32).
            if(hpre < 8)
            {
                return 0;
            }
            return hpre < 12 ? (int)hpre - 7 : (int)hpre - 6;
        }

        private void UpdateClocks()
        {
            var hclk = HclkFrequency;
            if(hclk == appliedHclk)
            {
                return;
            }
            appliedHclk = hclk;
            nvic.Frequency = hclk;
            // Takes effect when the CPU finishes its current time slice (BaseCPU seals it).
            cpu.PerformanceInMips = (uint)Math.Max(1UL, (hclk + 500000) / 1000000);
            this.Log(LogLevel.Info, "SYSCLK {0} Hz from {1}, HCLK {2} Hz: SysTick at HCLK, CPU {3} MIPS",
                SystemClockFrequency, systemClockSource, hclk, Math.Max(1UL, (hclk + 500000) / 1000000));
        }

        private IFlagRegisterField hsiOn;
        private IFlagRegisterField hseOn;
        private IFlagRegisterField pllOn;
        private IFlagRegisterField lseOn;
        private IFlagRegisterField lsiOn;
        private IValueRegisterField systemClockSwitch;
        private IValueRegisterField ahbPrescaler;
        private IFlagRegisterField pllSourceHse;
        private IFlagRegisterField hseDividedForPll;
        private IValueRegisterField pllMultiplier;
        private IValueRegisterField resetFlags;

        private ClockSource systemClockSource;
        private uint readyFlags;
        private uint previousReady;
        private ulong appliedHclk;

        private readonly NVIC nvic;
        private readonly BaseCPU cpu;
        private readonly ulong hseFrequency;

        private const ulong HsiFrequency = 8000000;

        private enum ClockSource
        {
            Hsi = 0,
            Hse = 1,
            Pll = 2,
            Invalid = 3
        }

        private enum Registers
        {
            Control = 0x00,
            Configuration = 0x04,
            ClockInterrupt = 0x08,
            Apb2Reset = 0x0C,
            Apb1Reset = 0x10,
            AhbEnable = 0x14,
            Apb2Enable = 0x18,
            Apb1Enable = 0x1C,
            BackupDomainControl = 0x20,
            ControlStatus = 0x24
        }
    }
}
