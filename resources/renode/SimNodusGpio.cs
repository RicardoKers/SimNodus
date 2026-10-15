//
// Copyright (c) 2010-2025 Antmicro
// Copyright (c) 2026 Ricardo Kerschbaumer (SimNodus changes)
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//
// STM32F1 GPIO port for SimNodus, derived from Renode's STM32F1GPIOPort
// (renode-infrastructure add012af). Differences, measured in ADR 0083/0085:
//   - CRL/CRH reset to 0x44444444 (floating inputs), as on the chip; the
//     original reset CNF to 00 and read back "analog";
//   - ODR is a real register: BSRR/BRR/ODR writes also set it for input pins,
//     so the pull-up/pull-down choice of CNF 10 is readable (the original
//     ignored those writes and read ODR back from the pin states);
//   - a pin that becomes a general-purpose output drives its ODR level;
//   - a BSRR write that resets and sets pins at once changes them in one step
//     (two separate fields produced zero-width glitches, found by E-07).
// Renode compiles this file at start-up (include), like LoopbackControl.cs.
using System.Collections.Generic;

using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Exceptions;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Utilities;

namespace Antmicro.Renode.Peripherals.GPIOPort
{
    [AllowedTranslations(AllowedTranslation.WordToDoubleWord)]
    public class SimNodusSTM32F1GPIOPort : BaseGPIOPort, IDoubleWordPeripheral, ILocalGPIOReceiver
    {
        public SimNodusSTM32F1GPIOPort(IMachine machine) : base(machine, NumberOfPins)
        {
            modes = new uint[NumberOfPins];
            configurations = new uint[NumberOfPins];
            AlternateFunctionOutputs = new GPIOAlternateFunction[NumberOfPins];
            for(var i = 0; i < NumberOfPins; i++)
            {
                AlternateFunctionOutputs[i] = new GPIOAlternateFunction(this, i);
            }

            var low = new DoubleWordRegister(this, 0x44444444);
            var high = new DoubleWordRegister(this, 0x44444444);
            for(var offset = 0; offset < 32; offset += 4)
            {
                var lowId = offset / 4;
                var highId = lowId + 8;
                low.WithValueField(offset, 2, name: $"MODE{lowId}",
                    writeCallback: (_, value) => SetMode(lowId, (uint)value), valueProviderCallback: _ => modes[lowId]);
                low.WithValueField(offset + 2, 2, name: $"CNF{lowId}",
                    writeCallback: (_, value) => SetConfiguration(lowId, (uint)value), valueProviderCallback: _ => configurations[lowId]);
                high.WithValueField(offset, 2, name: $"MODE{highId}",
                    writeCallback: (_, value) => SetMode(highId, (uint)value), valueProviderCallback: _ => modes[highId]);
                high.WithValueField(offset + 2, 2, name: $"CNF{highId}",
                    writeCallback: (_, value) => SetConfiguration(highId, (uint)value), valueProviderCallback: _ => configurations[highId]);
            }

            var registersMap = new Dictionary<long, DoubleWordRegister>
            {
                {(long)Registers.ConfigurationLow, low},
                {(long)Registers.ConfigurationHigh, high},
                {(long)Registers.InputData, new DoubleWordRegister(this)
                    .WithValueField(0, 16, FieldMode.Read, name: "IDR", valueProviderCallback: _ => BitHelper.GetValueFromBitsArray(State))
                },
                {(long)Registers.OutputData, new DoubleWordRegister(this)
                    .WithValueField(0, 16, name: "ODR", writeCallback: (_, value) => WriteOutputData((uint)value), valueProviderCallback: _ => outputData)
                },
                // One write applies BR and BS together (BS wins), as on the chip; two
                // separate fields would drive the pins low and high again (a glitch).
                {(long)Registers.BitSetReset, new DoubleWordRegister(this)
                    .WithValueField(0, 32, FieldMode.Write, name: "BSRR",
                        writeCallback: (_, value) => WriteOutputData((outputData & ~((uint)value >> 16)) | ((uint)value & 0xFFFFu)))
                },
                {(long)Registers.BitReset, new DoubleWordRegister(this)
                    .WithValueField(0, 16, FieldMode.Write, name: "BR", writeCallback: (_, value) => WriteOutputData(outputData & ~(uint)value))
                }
            };
            registers = new DoubleWordRegisterCollection(this, registersMap);
            Reset();
        }

        public override void OnGPIO(int number, bool value)
        {
            if(!CheckPinNumber(number))
            {
                return;
            }
            if(modes[number] != 0)
            {
                this.Log(LogLevel.Noisy, "Signal on output pin #{0} ignored", number);
                return;
            }
            base.OnGPIO(number, value);
            Connections[number].Set(value);
        }

        public override void Reset()
        {
            base.Reset();
            registers.Reset();
            outputData = 0;
            for(var i = 0; i < NumberOfPins; i++)
            {
                modes[i] = 0;          // input
                configurations[i] = 1; // floating
                AlternateFunctionOutputs[i].Reset();
            }
        }

        public IGPIOReceiver GetLocalReceiver(int pin)
        {
            if(pin < 0 || pin >= NumberOfPins)
            {
                throw new RecoverableException($"This peripheral supports GPIO inputs from 0 to {NumberOfPins - 1}, but {pin} was called.");
            }
            return AlternateFunctionOutputs[pin];
        }

        public uint ReadDoubleWord(long offset)
        {
            return registers.Read(offset);
        }

        public void WriteDoubleWord(long offset, uint value)
        {
            registers.Write(offset, value);
        }

        public GPIOAlternateFunction[] AlternateFunctionOutputs;

        private bool IsGeneralOutput(int pin)
        {
            return modes[pin] != 0 && configurations[pin] < 2;
        }

        private void WritePin(int number, bool value)
        {
            State[number] = value;
            Connections[number].Set(value);
        }

        private void WriteOutputData(uint value)
        {
            outputData = value & 0xFFFF;
            for(var i = 0; i < NumberOfPins; i++)
            {
                if(IsGeneralOutput(i))
                {
                    WritePin(i, (outputData & (1u << i)) != 0);
                }
            }
        }

        private void SetMode(int pin, uint mode)
        {
            var wasOutput = IsGeneralOutput(pin);
            modes[pin] = mode;
            UpdateAlternateFunction(pin);
            if(IsGeneralOutput(pin) && !wasOutput)
            {
                WritePin(pin, (outputData & (1u << pin)) != 0);
            }
        }

        private void SetConfiguration(int pin, uint configuration)
        {
            var wasOutput = IsGeneralOutput(pin);
            configurations[pin] = configuration;
            UpdateAlternateFunction(pin);
            if(IsGeneralOutput(pin) && !wasOutput)
            {
                WritePin(pin, (outputData & (1u << pin)) != 0);
            }
        }

        private void UpdateAlternateFunction(int pin)
        {
            AlternateFunctionOutputs[pin].IsConnected = modes[pin] != 0 && configurations[pin] >= 2;
        }

        private readonly DoubleWordRegisterCollection registers;
        private readonly uint[] modes;          // MODE bits: 0 input, 1..3 output speeds
        private readonly uint[] configurations; // CNF bits
        private uint outputData;

        private const int NumberOfPins = 16;

        public class GPIOAlternateFunction : IGPIOReceiver
        {
            public GPIOAlternateFunction(SimNodusSTM32F1GPIOPort port, int pin)
            {
                this.port = port;
                this.pin = pin;
                Reset();
            }

            public void Reset()
            {
                IsConnected = false;
                ActiveFunction = 0;
            }

            public void OnGPIO(int number, bool value)
            {
                if(!IsConnected || number != (int)ActiveFunction)
                {
                    return;
                }
                port.WritePin(pin, value);
            }

            public bool IsConnected { get; set; }

            public ulong ActiveFunction { get; set; }

            private readonly SimNodusSTM32F1GPIOPort port;
            private readonly int pin;
        }

        private enum Registers
        {
            ConfigurationLow = 0x00,
            ConfigurationHigh = 0x04,
            InputData = 0x08,
            OutputData = 0x0C,
            BitSetReset = 0x10,
            BitReset = 0x14,
            PortConfigurationLock = 0x18
        }
    }
}
