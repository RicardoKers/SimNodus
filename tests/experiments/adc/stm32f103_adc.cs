// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT

using System;

using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Exceptions;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.Sensor;
using Antmicro.Renode.Peripherals.Timers;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Peripherals
{
    // Focused E-04 model: one regular software-triggered STM32F103 conversion.
    // This is intentionally smaller than the hardware peripheral.
    public class SimNodusSTM32F103ADC : BasicDoubleWordPeripheral, IKnownSize, IADC
    {
        public SimNodusSTM32F103ADC(IMachine machine, uint referenceMicrovolts = 3300000,
            long adcClockFrequency = 1000000) : base(machine)
        {
            if(referenceMicrovolts == 0 || adcClockFrequency <= 0)
            {
                throw new RecoverableException("ADC reference and clock must be positive");
            }

            this.referenceMicrovolts = referenceMicrovolts;
            inputMicrovolts = new uint[ExternalChannelCount];
            conversionTimer = new LimitTimer(machine.ClockSource, (ulong)adcClockFrequency, this,
                "conversionClock", limit: ConversionCycles[0], eventEnabled: true,
                direction: Direction.Ascending, enabled: false, autoUpdate: false,
                workMode: WorkMode.OneShot);
            conversionTimer.LimitReached += CompleteConversion;
            DefineRegisters();
        }

        public override void Reset()
        {
            base.Reset();
            conversionTimer.Enabled = false;
            Array.Clear(inputMicrovolts, 0, inputMicrovolts.Length);
            data = 0;
            pendingCode = 0;
            IRQ.Set(false);
        }

        public void SetADCValue(int channel, uint value)
        {
            AssertChannel(channel);
            lock(inputLock)
            {
                inputMicrovolts[channel] = value;
            }
        }

        public uint GetADCValue(int channel)
        {
            AssertChannel(channel);
            lock(inputLock)
            {
                return inputMicrovolts[channel];
            }
        }

        public int ADCChannelCount => ExternalChannelCount;

        public long Size => 0x50;

        public GPIO IRQ { get; } = new GPIO();

        private void AssertChannel(int channel)
        {
            if(channel < 0 || channel >= ExternalChannelCount)
            {
                throw new RecoverableException($"ADC channel must be in [0, {ExternalChannelCount - 1}]");
            }
        }

        private void DefineRegisters()
        {
            Registers.Status.Define(this)
                .WithTaggedFlag("Analog watchdog", 0)
                .WithFlag(1, out endOfConversion, name: "End of regular conversion")
                .WithTaggedFlag("Injected end of conversion", 2)
                .WithTaggedFlag("Injected conversion started", 3)
                .WithFlag(4, out regularConversionStarted, name: "Regular conversion started")
                .WithReservedBits(5, 27);

            Registers.Control1.Define(this)
                .WithReservedBits(0, 5)
                .WithFlag(5, out endOfConversionInterruptEnable, name: "EOC interrupt enable")
                .WithTaggedFlag("Analog watchdog interrupt enable", 6)
                .WithTaggedFlag("Injected EOC interrupt enable", 7)
                .WithTaggedFlag("Scan mode", 8)
                .WithReservedBits(9, 23);

            Registers.Control2.Define(this)
                .WithFlag(0, out adcEnabled, name: "ADC enable")
                .WithTaggedFlag("Continuous conversion", 1)
                .WithTaggedFlag("Calibration", 2)
                .WithTaggedFlag("Reset calibration", 3)
                .WithReservedBits(4, 4)
                .WithTaggedFlag("DMA", 8)
                .WithReservedBits(9, 2)
                .WithFlag(11, out leftAlignment, name: "Data alignment")
                .WithReservedBits(12, 8)
                .WithFlag(20, name: "External trigger enable")
                .WithTaggedFlag("Injected software start", 21)
                .WithFlag(22, name: "Regular software start",
                    writeCallback: (_, value) => { if(value) StartConversion(); },
                    valueProviderCallback: _ => false)
                .WithTaggedFlag("Temperature/VREF enable", 23)
                .WithReservedBits(24, 8);

            Registers.SampleTime1.Define(this)
                .WithTag("Channels 10 through 17 sampling times", 0, 24)
                .WithReservedBits(24, 8);

            Registers.SampleTime2.Define(this)
                .WithValueField(0, 3, out channel0SampleTime, name: "Channel 0 sampling time")
                .WithValueField(3, 3, out channel1SampleTime, name: "Channel 1 sampling time")
                .WithTag("Channels 2 through 9 sampling times", 6, 24)
                .WithReservedBits(30, 2);

            Registers.RegularSequence1.Define(this).WithTag("Regular sequence and length", 0, 24).WithReservedBits(24, 8);
            Registers.RegularSequence2.Define(this).WithTag("Regular sequence", 0, 30).WithReservedBits(30, 2);
            Registers.RegularSequence3.Define(this)
                .WithValueField(0, 5, out firstRegularChannel, name: "First regular channel")
                .WithTag("Unsupported regular ranks", 5, 25)
                .WithReservedBits(30, 2);

            Registers.RegularData.Define(this)
                .WithValueField(0, 16, valueProviderCallback: _ => ReadData())
                .WithReservedBits(16, 16);
        }

        private void StartConversion()
        {
            if(!adcEnabled.Value)
            {
                this.Log(LogLevel.Warning, "Ignoring software start while ADC is disabled");
                return;
            }
            if(conversionTimer.Enabled)
            {
                this.Log(LogLevel.Warning, "Ignoring software start while a conversion is active");
                return;
            }

            var channel = (int)firstRegularChannel.Value;
            if(channel >= ExternalChannelCount)
            {
                this.Log(LogLevel.Warning, "Unsupported regular channel {0}", channel);
                pendingCode = 0;
            }
            else
            {
                uint sample;
                lock(inputLock)
                {
                    sample = inputMicrovolts[channel];
                }
                pendingCode = Quantize(sample);
            }

            var sampleTime = channel == 1 ? channel1SampleTime.Value : channel0SampleTime.Value;
            conversionTimer.Limit = ConversionCycles[(int)sampleTime];
            endOfConversion.Value = false;
            regularConversionStarted.Value = true;
            IRQ.Set(false);
            conversionTimer.Enabled = true;
        }

        private void CompleteConversion()
        {
            data = pendingCode;
            endOfConversion.Value = true;
            if(endOfConversionInterruptEnable.Value)
            {
                IRQ.Set(true);
            }
        }

        private uint ReadData()
        {
            endOfConversion.Value = false;
            IRQ.Set(false);
            return leftAlignment.Value ? data << 4 : data;
        }

        private uint Quantize(uint microvolts)
        {
            var scaled = (ulong)microvolts * 4096UL / referenceMicrovolts;
            return (uint)Math.Min(4095UL, scaled);
        }

        private uint data;
        private uint pendingCode;
        private readonly uint referenceMicrovolts;
        private readonly uint[] inputMicrovolts;
        private readonly object inputLock = new object();
        private readonly LimitTimer conversionTimer;
        private IFlagRegisterField endOfConversion;
        private IFlagRegisterField regularConversionStarted;
        private IFlagRegisterField endOfConversionInterruptEnable;
        private IFlagRegisterField adcEnabled;
        private IFlagRegisterField leftAlignment;
        private IValueRegisterField channel0SampleTime;
        private IValueRegisterField channel1SampleTime;
        private IValueRegisterField firstRegularChannel;

        private const int ExternalChannelCount = 16;
        private static readonly ulong[] ConversionCycles = { 14, 20, 26, 41, 54, 68, 84, 252 };

        private enum Registers
        {
            Status = 0x00,
            Control1 = 0x04,
            Control2 = 0x08,
            SampleTime1 = 0x0C,
            SampleTime2 = 0x10,
            RegularSequence1 = 0x2C,
            RegularSequence2 = 0x30,
            RegularSequence3 = 0x34,
            RegularData = 0x4C,
        }
    }
}
