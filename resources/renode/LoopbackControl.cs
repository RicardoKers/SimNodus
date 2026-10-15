//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System;
using System.Collections.Generic;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Sensor;
using Antmicro.Renode.Utilities;
using Antmicro.Renode;
    public class ADC : BaseCommand, IInstanceBasedCommand<IADC>
    {
        public ADC(ExternalControlServer parent)
            : base(parent)
        {
            Instances = new InstanceCollection<IADC>();
        }

        public override Response Invoke(List<byte> data) => this.InvokeHandledWithInstance(data);

        public Response Invoke(IADC instance, List<byte> data)
        {
            if(data.Count < 1)
            {
                return Response.CommandFailed(Identifier, $"Expected at least {1 + InstanceBasedCommandHeaderSize} bytes of payload");
            }
            var command = (ADCCommand)data[0];

            var expectedCount = GetExpectedPayloadCount(command);
            if(expectedCount != data.Count)
            {
                return Response.CommandFailed(Identifier, $"Expected {expectedCount + InstanceBasedCommandHeaderSize} bytes of payload");
            }

            switch(command)
            {
            case ADCCommand.GetCount:
                var channelCount = instance.ADCChannelCount;
                parent.Log(LogLevel.Debug, "Executing ADC GetCount command, returned {0}", channelCount);
                return Response.Success(Identifier, channelCount.AsRawBytes());

            case ADCCommand.GetValue:
                DecodeChannelArgument(data, out var channel);
                var value = instance.GetADCValue(channel);
                parent.Log(LogLevel.Debug, "Executing ADC GetValue command, channel #{0} returned {1}", channel, value);
                return Response.Success(Identifier, value.AsRawBytes());

            case ADCCommand.SetValue:
                DecodeSetValueArguments(data, out channel, out value);
                parent.Log(LogLevel.Debug, "Executing ADC SetValue command, channel #{0} set to {1}", channel, value);
                instance.SetADCValue(channel, value);
                return Response.Success(Identifier);

            default:
                return Response.CommandFailed(Identifier, "Unexpected command format");
            }
        }

        public InstanceCollection<IADC> Instances { get; }

        public override Command Identifier => Command.ADC;

        public override byte Version => 0x0;

        private int GetExpectedPayloadCount(ADCCommand command)
        {
            switch(command)
            {
            case ADCCommand.GetValue:
                return sizeof(byte) + sizeof(uint);
            case ADCCommand.SetValue:
                return sizeof(byte) + sizeof(uint) * 2;
            default:
                return sizeof(byte);
            }
        }

        private void DecodeChannelArgument(List<byte> data, out int channel)
        {
            channel = BitConverter.ToInt32(data.GetRange(1, sizeof(uint)).ToArray(), 0);
        }

        private void DecodeSetValueArguments(List<byte> data, out int channel, out uint value)
        {
            DecodeChannelArgument(data, out channel);
            value = BitConverter.ToUInt32(data.GetRange(5, sizeof(uint)).ToArray(), 0);
        }

        private const int InstanceBasedCommandHeaderSize = IInstanceBasedCommandExtensions.HeaderSize;

        private enum ADCCommand : byte
        {
            GetCount = 0,
            GetValue,
            SetValue,
        }
    }
}
//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode
{
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Antmicro.Renode.Core;
using Antmicro.Renode.Exceptions;
using Antmicro.Renode.Logging;
using SimNodus.Renode.ExternalControl;
using Antmicro.Renode.Utilities;
using Antmicro.Renode.Utilities.Packets;
using Antmicro.Renode;
    public static class ExternalControlServerExtensions
    {
        public static void CreateLoopbackControlServer(this Emulation emulation, string name, int port)
        {
            emulation.ExternalsManager.AddExternal(new ExternalControlServer(port), name);
        }
    }

    public class ExternalControlServer : IDisposable, IExternal, IEmulationElement
    {
        public ExternalControlServer(int port)
        {
            socketServerProvider.BufferSize = 0x10;

            commandHandlers = new CommandHandlerCollection();
            commandHandlers.EventReported += SendEventResponse;
            commandHandlers.Register(new RunFor(this));
            commandHandlers.Register(new GetTime(this));
            commandHandlers.Register(new ADC(this));
            commandHandlers.Register(new GPIOPort(this));
            commandHandlers.Register(new SystemBus(this));
            commandHandlers.Register(new UART(this));

            var getMachineHandler = new GetMachine(this);
            Machines = getMachineHandler;
            commandHandlers.Register(getMachineHandler);

            socketServerProvider.ConnectionAccepted += delegate
            {
                lock(locker)
                {
                    if(state == State.Disposed)
                    {
                        return;
                    }
                    this.Log(LogLevel.Noisy, "State change: {0} -> {1}", state, State.Handshake);
                    state = State.Handshake;
                }
                this.Log(LogLevel.Debug, "Connection established");
            };
            socketServerProvider.ConnectionClosed += delegate
            {
                lock(locker)
                {
                    if(state == State.Disposed)
                    {
                        return;
                    }
                    commandHandlers.ClearActivation();
                    this.Log(LogLevel.Noisy, "State change: {0} -> {1}", state, State.NotConnected);
                    state = State.NotConnected;
                }
                this.Log(LogLevel.Debug, "Connection closed");
            };

            socketServerProvider.DataBlockReceived += OnBytesWritten;
            socketServerProvider.Start(port);

            this.Log(LogLevel.Info, "{0}: Listening on port {1}", nameof(ExternalControlServer), port);
        }

        public void Dispose()
        {
            lock(locker)
            {
                this.Log(LogLevel.Noisy, "State change: {0} -> {1}", state, State.Disposed);
                state = State.Disposed;
            }
            commandHandlers.Dispose();
            socketServerProvider.Stop();
        }

        public IMachineContainer Machines { get; }

        public bool EventsEnabled = false;

        private bool IsHeaderValid()
        {
            if(!header.HasValue)
            {
                return false;
            }

            if(header.Value.Magic != Magic)
            {
                return false;
            }

            if(header.Value.DataSize > (uint)Int32.MaxValue)
            {
                return false;
            }

            return true;
        }

        private bool TryActivateCommands(List<byte> data)
        {
            foreach(var pair in data.Split(2))
            {
                var command = (Command)pair[0];
                var version = pair[1];

                if(commandHandlers.TryActivate(command, version))
                {
                    this.Log(LogLevel.Noisy, "{0} (version 0x{1:X}) activated", command, version);
                    continue;
                }

                var message = commandHandlers.TryGetVersion(command, out var expectedVersion)
                    ? $"Encountered invalid version (0x{version:X}) for {command}, expected 0x{expectedVersion:X}"
                    : $"Encountered unknown command 0x{command:X}";

                this.Log(LogLevel.Error, message);
                SendResponse(Response.FatalError(message));
                return false;
            }

            return true;
        }

        private State? StepReceiveFiniteStateMachine(State currentState)
        {
            switch(currentState)
            {
            case State.Handshake:
                if(buffer.Count < HandshakeHeaderSize)
                {
                    return null;
                }
                commandsToActivate = BitConverter.ToUInt16(buffer.GetRange(0, HandshakeHeaderSize).ToArray(), 0);
                buffer.RemoveRange(0, HandshakeHeaderSize);
                this.Log(LogLevel.Noisy, "{0} commands to activate", commandsToActivate);

                return State.WaitingForHandshakeData;

            case State.WaitingForHandshakeData:
                if(commandsToActivate > 0 && buffer.Count >= 2)
                {
                    var toActivate = (int)Math.Min(commandsToActivate, buffer.Count / 2);

                    lock(locker)
                    {
                        AssertNotDisposed();
                        if(!TryActivateCommands(buffer.GetRange(0, toActivate * 2)))
                        {
                            socketServerProvider.Stop();
                            return null;
                        }
                    }

                    buffer.RemoveRange(0, toActivate * 2);
                    commandsToActivate -= toActivate;
                }

                if(commandsToActivate > 0)
                {
                    return null;
                }

                SendResponse(Response.SuccessfulHandshake());

                return State.WaitingForHeader;

            case State.WaitingForHeader:
                if(buffer.Count < HeaderSize)
                {
                    return null;
                }

                header = Packet.Decode<ExternalControlProtocolHeader>(buffer);
                if(!IsHeaderValid())
                {
                    var message = $"Encountered invalid header: {header}";
                    this.Log(LogLevel.Error, message);
                    lock(locker)
                    {
                        SendResponse(Response.FatalError(message));
                        socketServerProvider.Stop();
                    }
                    return null;
                }

                this.Log(LogLevel.Noisy, "Received header: {0}", header);
                buffer.RemoveRange(0, HeaderSize);

                return State.WaitingForData;

            case State.WaitingForData:
                if(buffer.Count < header.Value.DataSize)
                {
                    return null;
                }

                TryHandleCommand(out var response, header.Value.Command, buffer.GetRange(0, (int)header.Value.DataSize));

                buffer.RemoveRange(0, (int)header.Value.DataSize);
                header = null;

                SendResponse(response);

                return State.WaitingForHeader;

            case State.NotConnected:
            default:
                throw new Exception("Unreachable");
            }
        }

        private void OnBytesWritten(byte[] data)
        {
            buffer.AddRange(data);
            this.Log(LogLevel.Noisy, "Received new data: {0}", Misc.PrettyPrintCollectionHex(data));
            this.Log(LogLevel.Debug, "Current buffer: {0}", Misc.PrettyPrintCollectionHex(buffer));

            var lockedState = state;
            while(lockedState != State.Disposed && lockedState != State.NotConnected)
            {
                var nextState = (State?)null;
                try
                {
                    nextState = StepReceiveFiniteStateMachine(lockedState);
                }
                catch(ServerDisposedException)
                {
                    return;
                }

                lock(locker)
                {
                    if(!nextState.HasValue || state == State.Disposed || state == State.NotConnected)
                    {
                        return;
                    }
                    this.Log(LogLevel.Noisy, "State change: {0} -> {1}", state, nextState.Value);
                    state = nextState.Value;
                    lockedState = state;
                }
            }
        }

        private bool TryHandleCommand(out Response response, Command command, List<byte> data)
        {
            ICommand commandHandler;
            lock(locker)
            {
                AssertNotDisposed();
                commandHandler = commandHandlers.GetHandler(command);
            }

            if(commandHandler == null)
            {
                response = Response.InvalidCommand(command);
                return true;
            }

            try
            {
                // Create an ICanReportEvents interface or something similar if
                // we ever have more command handlers that need to do this.
                if(commandHandler is RunFor)
                {
                    EventsEnabled = true;
                }

                response = commandHandler.Invoke(data);
                return true;
            }
            catch(RecoverableException e)
            {
                this.Log(LogLevel.Error, "{0} command error: {1}", command, e.Message);
                response = Response.CommandFailed(command, e.Message);
                return false;
            }
            finally
            {
                if(commandHandler is RunFor)
                {
                    EventsEnabled = false;
                }
            }
        }

        private void SendEventResponse(Response response)
        {
            if(EventsEnabled)
            {
                SendResponse(response);
            }
        }

        private void SendResponse(Response response)
        {
            var bytes = response.GetBytes();
            lock(locker)
            {
                AssertNotDisposed();
                socketServerProvider.Send(bytes);
            }
            this.Log(LogLevel.Debug, "Response sent: {0}", response);
            this.Log(LogLevel.Noisy, "Bytes sent: {0}", Misc.PrettyPrintCollectionHex(bytes));
        }

        private void AssertNotDisposed()
        {
            if(state == State.Disposed)
            {
                throw new ServerDisposedException();
            }
        }

        private State state = State.NotConnected;
        private int commandsToActivate = 0;
        private ExternalControlProtocolHeader? header;

        private readonly List<byte> buffer = new List<byte>();
        private readonly CommandHandlerCollection commandHandlers;
        private readonly SimNodusSocketServerProvider socketServerProvider = new SimNodusSocketServerProvider(telnetMode: false);

        private readonly object locker = new object();

        private const int HeaderSize = 7;
        private const int HandshakeHeaderSize = 2;
        private const string Magic = "RE";

        private class CommandHandlerCollection : IDisposable
        {
            public CommandHandlerCollection()
            {
                commandHandlers = new Dictionary<Command, ICommand>();
                activeCommandHandlers = new Dictionary<Command, ICommand>();
            }

            public void Dispose()
            {
                activeCommandHandlers.Clear();
                foreach(var command in commandHandlers.Values.OfType<IDisposable>())
                {
                    command.Dispose();
                }
                commandHandlers.Clear();
            }

            public void Register(ICommand command)
            {
                commandHandlers.Add(command.Identifier, command);
                if(command is IHasEvents commandWithEvents)
                {
                    commandWithEvents.EventReported += this.EventReported;
                }
            }

            public void ClearActivation()
            {
                activeCommandHandlers.Clear();
            }

            public bool TryActivate(Command id, byte version)
            {
                if(activeCommandHandlers.ContainsKey(id))
                {
                    return false;
                }

                if(!commandHandlers.TryGetValue(id, out var command))
                {
                    return false;
                }

                if(command.Version != version)
                {
                    return false;
                }

                activeCommandHandlers.Add(command.Identifier, command);
                return true;
            }

            public ICommand GetHandler(Command id)
            {
                if(!activeCommandHandlers.TryGetValue(id, out var command))
                {
                    return null; ;
                }
                return command;
            }

            public bool TryGetVersion(Command id, out byte version)
            {
                if(!commandHandlers.TryGetValue(id, out var command))
                {
                    version = default(byte);
                    return false;
                }

                version = command.Version;
                return true;
            }

            public event Action<Response> EventReported;

            private readonly Dictionary<Command, ICommand> commandHandlers;
            private readonly Dictionary<Command, ICommand> activeCommandHandlers;
        }

        private class ServerDisposedException : RecoverableException
        {
            public ServerDisposedException()
                : base()
            {
            }
        }

        [LeastSignificantByteFirst]
        private struct ExternalControlProtocolHeader
        {
            public override string ToString()
            {
                return $"{{ magic: {Misc.PrettyPrintCollectionHex(MagicField)} ({Magic}), command: 0x{(byte)Command} ({Command}), dataSize: {DataSize} }}";
            }

            public string Magic
            {
                get
                {
                    try
                    {
                        return Encoding.ASCII.GetString(MagicField);
                    }
                    catch
                    {
                        return "<invalid>";
                    }
                }
            }

#pragma warning disable 649
            [PacketField, Width(bytes: 2)]
            public byte[] MagicField;
            [PacketField, Width(bits: 8)]
            public Command Command;
            [PacketField, Width(bits: 32)]
            public uint DataSize;
#pragma warning restore 649
        }

        private enum State
        {
            NotConnected,
            Handshake,
            WaitingForHandshakeData,
            WaitingForHeader,
            WaitingForData,
            Disposed,
        }
    }
}

//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Utilities;
using Antmicro.Renode;
    public class GetMachine : BaseCommand, IMachineContainer
    {
        public GetMachine(ExternalControlServer parent)
            : base(parent)
        {
            machines = new InstanceCollection<IMachine>();
        }

        public bool TryGetMachine(int id, out IMachine machine) => machines.TryGet(id, out machine);

        public override Response Invoke(List<byte> data)
        {
            if(!IInstanceBasedCommandExtensions.TryGetName(Identifier, data, 0, out var name, out var response))
            {
                return response;
            }

            if(!EmulationManager.Instance.CurrentEmulation.TryGetMachineByName(name, out var machine))
            {
                return Response.CommandFailed(Identifier, "Machine not found");
            }

            machines.TryAdd(machine, out var id);
            return Response.Success(Identifier, id.AsRawBytes());
        }

        public override Command Identifier => Command.GetMachine;

        public override byte Version => 0x0;

        private readonly InstanceCollection<IMachine> machines;
    }

    public interface IMachineContainer
    {
        bool TryGetMachine(int id, out IMachine machine);
    }
}
//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Utilities;
using Antmicro.Renode;
    public class GetTime : BaseCommand
    {
        public GetTime(ExternalControlServer parent)
            : base(parent)
        {
        }

        public override Response Invoke(List<byte> data)
        {
            var timestamp = EmulationManager.Instance.CurrentEmulation.MasterTimeSource.ElapsedVirtualTime;
            parent.Log(LogLevel.Info, "Executing GetTime command: {0}", timestamp);

            return Response.Success(Identifier, ((ulong)timestamp.TotalMicroseconds).AsRawBytes());
        }

        public override Command Identifier => Command.GetTime;

        public override byte Version => 0x0;
    }
}
//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System;
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Utilities;
using Antmicro.Renode;
    public class GPIOPort : BaseCommand, IHasEvents, IInstanceBasedCommand<IPeripheral>
    {
        public GPIOPort(ExternalControlServer parent)
            : base(parent)
        {
            Instances = new InstanceCollection<IPeripheral>();
        }

        public override Response Invoke(List<byte> data) => this.InvokeHandledWithInstance(data, HasGPIO);

        public Response Invoke(IPeripheral instance, List<byte> data)
        {
            if(data.Count < 1)
            {
                return Response.CommandFailed(Identifier, $"Expected at least {1 + InstanceBasedCommandHeaderSize} bytes of payload");
            }
            var command = (GPIOPortCommand)data[0];

            var expectedCount = GetExpectedPayloadCount(command);
            if(expectedCount != data.Count)
            {
                return Response.CommandFailed(Identifier, $"Expected {expectedCount + InstanceBasedCommandHeaderSize} bytes of payload");
            }

            switch(command)
            {
            case GPIOPortCommand.GetState:
                DecodeIdArgument(data, out var id);

                if(instance is INumberedGPIOOutput sender)
                {
                    if(sender.Connections.TryGetValue(id, out var gpio))
                    {
                        return Response.Success(Identifier, BitConverter.GetBytes(gpio.IsSet));
                    }
                    return Response.CommandFailed(Identifier, $"This instance does not provide GPIO output #{id}");
                }
                return Response.CommandFailed(Identifier, "This instance does not provide GPIO outputs");

            case GPIOPortCommand.SetState:
                DecodeSetValueArguments(data, out id, out var value);

                if(instance is IGPIOReceiver receiver)
                {
                    receiver.OnGPIO(id, value);
                    return Response.Success(Identifier);
                }
                return Response.CommandFailed(Identifier, "This instance does not provide GPIO inputs");

            case GPIOPortCommand.RegisterEvent:
                DecodeRegisterEventArguments(data, out id, out var ed);

                if(!(instance is INumberedGPIOOutput gpioDriver))
                {
                    return Response.CommandFailed(Identifier, "This instance does not provide GPIO outputs");
                }

                if(!gpioDriver.Connections.TryGetValue(id, out var pin) || !(pin is GPIO))
                {
                    return Response.CommandFailed(Identifier, $"This instance does not provide GPIO output #{id}");
                }

                    (pin as GPIO).AddStateChangedHook((state) => SendEvent(state, ed));
                return Response.Success(Identifier);

            default:
                return Response.CommandFailed(Identifier, "Unexpected command format");
            }
        }

        public override Command Identifier => Command.GPIOPort;

        public override byte Version => 0x1;

        public InstanceCollection<IPeripheral> Instances { get; }

        public event Action<Response> EventReported;

        private static bool HasGPIO(IPeripheral instance)
        {
            return instance is INumberedGPIOOutput || instance is IGPIOReceiver;
        }

        private void SendEvent(bool gpioState, int eventDescriptor)
        {
            var data = new EventData();
            data.TimestampMicroseconds = (ulong)EmulationManager.Instance.CurrentEmulation.MasterTimeSource.ElapsedVirtualTime.TotalMicroseconds;
            data.GpioState = gpioState;

            EventReported?.Invoke(Response.Event(Identifier, eventDescriptor, data.AsRawBytes()));
        }

        private int GetExpectedPayloadCount(GPIOPortCommand command)
        {
            switch(command)
            {
            case GPIOPortCommand.GetState:
                return sizeof(byte) + sizeof(uint);
            case GPIOPortCommand.SetState:
                return sizeof(byte) * 2 + sizeof(uint);
            case GPIOPortCommand.RegisterEvent:
                return sizeof(byte) + sizeof(uint) * 2;
            default:
                return sizeof(byte);
            }
        }

        private void DecodeIdArgument(List<byte> data, out int id)
        {
            id = BitConverter.ToInt32(data.GetRange(1, sizeof(uint)).ToArray(), 0);
        }

        private void DecodeSetValueArguments(List<byte> data, out int id, out bool value)
        {
            DecodeIdArgument(data, out id);
            value = BitConverter.ToBoolean(data.GetRange(5, sizeof(byte)).ToArray(), 0);
        }

        private void DecodeRegisterEventArguments(List<byte> data, out int id, out int ed)
        {
            DecodeIdArgument(data, out id);
            ed = BitConverter.ToInt32(data.GetRange(5, sizeof(uint)).ToArray(), 0);
        }

        private const int InstanceBasedCommandHeaderSize = IInstanceBasedCommandExtensions.HeaderSize;

        private struct EventData
        {
            public ulong TimestampMicroseconds;
            public bool GpioState;
        }

        private enum GPIOPortCommand : byte
        {
            GetState = 0,
            SetState,
            RegisterEvent,
        }
    }
}
//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//

namespace SimNodus.Renode.ExternalControl
{
using System;
using System.Collections.Generic;
using Antmicro.Renode;
    public enum Command : byte
    {
        RunFor = 1,
        GetTime,
        GetMachine,
        ADC,
        GPIOPort,
        SystemBus,
        UART, // SimNodus addition (SN-134)
    }

    public interface ICommand
    {
        Command Identifier { get; }

        byte Version { get; }

        IMachineContainer Machines { get; }

        Response Invoke(List<byte> data);
    }

    public abstract class BaseCommand : ICommand
    {
        public BaseCommand(ExternalControlServer parent)
        {
            this.parent = parent;
        }

        public abstract Response Invoke(List<byte> data);

        public IMachineContainer Machines => parent.Machines;

        public abstract Command Identifier { get; }

        public abstract byte Version { get; }

        protected readonly ExternalControlServer parent;
    }

    public interface IHasEvents : ICommand
    {
        event Action<Response> EventReported;
    }
}
//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System;
using System.Collections.Generic;
using System.Text;
using Antmicro.Renode.Core;
using Antmicro.Renode.Utilities;
using Antmicro.Renode;
    public static class IInstanceBasedCommandExtensions
    {
        public static bool TryGetName(Command command, List<byte> data, int offset, out string name, out Response response)
        {
            name = default(string);
            response = default(Response);

            if(!TryDecodeNameLength(command, data, offset, out var length, out response))
            {
                return false;
            }

            return TryDecodeName(command, data, offset + NameLengthSize, length, out name, out response);
        }

        public static Response InvokeHandledWithInstance<T>(this IInstanceBasedCommand<T> @this, List<byte> data, Predicate<T> instanceFilter = null)
            where T : IEmulationElement
        {
            if(data.Count < PayloadOffset)
            {
                return Response.CommandFailed(@this.Identifier, $"Expected at least {PayloadOffset} bytes of payload");
            }
            var id = BitConverter.ToInt32(data.GetRange(InstanceIdOffset, InstanceIdSize).ToArray(), 0);

            // only non-negative instance ids are valid
            if(id >= 0 && @this.Instances.TryGet(id, out var instance))
            {
                return @this.Invoke(instance, data.GetRange(PayloadOffset, data.Count - PayloadOffset));
            }

            // id set to a magic of -1 is used to register a new instance
            if(id != -1)
            {
                return Response.CommandFailed(@this.Identifier, "Invalid instance id");
            }

            // requested instance registration

            if(!TryGetMachine(@this, data, out var machine, out var response))
            {
                return response;
            }

            if(!TryGetName(@this.Identifier, data, NameOffset, out var name, out response))
            {
                return response;
            }

            if(!@this.TryRegisterInstance(machine, name, instanceFilter, out id))
            {
                return Response.CommandFailed(@this.Identifier, "Instance not found");
            }

            return Response.Success(@this.Identifier, id.AsRawBytes());
        }

        public const int HeaderSize = InstanceIdSize;

        private static bool TryRegisterInstance<T>(this IInstanceBasedCommand<T> @this, IMachine machine, string name, Predicate<T> instanceFilter, out int id)
            where T : IEmulationElement
        {
            id = default(int);
            if(!EmulationManager.Instance.CurrentEmulation.TryGetEmulationElementByName(name, machine, out var instance))
            {
                return false;
            }

            if(!(instance is T) || (!instanceFilter?.Invoke((T)instance) ?? false))
            {
                return false;
            }

            @this.Instances.TryAdd((T)instance, out id);
            return true;
        }

        private static bool TryGetMachine(this ICommand @this, List<byte> data, out IMachine machine, out Response response)
        {
            machine = default(IMachine);
            response = default(Response);

            if(data.Count < MachineIdOffset + MachineIdSize)
            {
                response = Response.CommandFailed(@this.Identifier, $"Expected at least {MachineIdOffset + MachineIdSize} bytes of payload");
                return false;
            }

            var id = BitConverter.ToInt32(data.GetRange(MachineIdOffset, MachineIdSize).ToArray(), 0);
            if(@this.Machines?.TryGetMachine(id, out machine) ?? false)
            {
                return true;
            }

            response = Response.CommandFailed(@this.Identifier, "Invalid machine id");
            return false;
        }

        private static bool TryDecodeNameLength(Command command, List<byte> data, int offset, out int length, out Response response)
        {
            response = default(Response);

            if(data.Count < offset + NameLengthSize)
            {
                length = default(int);
                response = Response.CommandFailed(command, $"Expected at least {offset + NameLengthSize} bytes of payload");
                return false;
            }

            length = BitConverter.ToInt32(data.GetRange(offset, NameLengthSize).ToArray(), 0);

            if(length <= 0)
            {
                response = Response.CommandFailed(command, "Provided length value is illegal");
                return false;
            }

            return true;
        }

        private static bool TryDecodeName(Command command, List<byte> data, int offset, int length, out string name, out Response response)
        {
            name = default(string);
            response = default(Response);

            if(data.Count != offset + length)
            {
                response = Response.CommandFailed(command, $"Expected {offset + length} bytes of payload");
                return false;
            }

            try
            {
                name = Encoding.UTF8.GetString(data.GetRange(offset, length).ToArray());
            }
            catch(Exception)
            {
                response = Response.CommandFailed(command, "Invalid name encoding");
                return false;
            }

            return true;
        }

        private const int PayloadOffset = InstanceIdOffset + InstanceIdSize;
        private const int InstanceIdOffset = 0;
        private const int InstanceIdSize = 4;
        private const int MachineIdOffset = InstanceIdSize;
        private const int MachineIdSize = 4;
        private const int NameOffset = MachineIdOffset + MachineIdSize;
        private const int NameLengthSize = 4;
    }

    public interface IInstanceBasedCommand<T> : ICommand
        where T : IEmulationElement
    {
        InstanceCollection<T> Instances { get; }

        Response Invoke(T instance, List<byte> data);
    }

    public class InstanceCollection<T>
    {
        public InstanceCollection()
        {
            collection = new Dictionary<int, T>();
        }

        public T this[int id] => collection[id];

        public bool TryAdd(T instance, out int id)
        {
            id = nextId;

            if(nextId < 0)
            {
                return false;
            }
            nextId += 1;

            collection.Add(id, instance);
            return true;
        }

        public void Remove(int id) => collection.Remove(id);

        public bool TryGet(int id, out T instance) => collection.TryGetValue(id, out instance);

        private int nextId;
        private readonly Dictionary<int, T> collection;
    }
}
//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Antmicro.Renode.Debugging;
using Antmicro.Renode.Utilities;
using Antmicro.Renode;
    public class Response
    {
        public static Response CommandFailed(Command command, string reason)
        {
            return new Response(ReturnCode.CommandFailed, command, text: reason);
        }

        public static Response FatalError(string reason)
        {
            return new Response(ReturnCode.FatalError, text: reason);
        }

        public static Response InvalidCommand(Command command)
        {
            return new Response(ReturnCode.InvalidCommand, command);
        }

        public static Response Success(Command command)
        {
            return new Response(ReturnCode.SuccessWithoutData, command);
        }

        public static Response Success(Command command, IEnumerable<byte> data)
        {
            return new Response(ReturnCode.SuccessWithData, command, data);
        }

        public static Response Success(Command command, string text)
        {
            return new Response(ReturnCode.SuccessWithData, command, text);
        }

        public static Response SuccessfulHandshake()
        {
            return new Response(ReturnCode.SuccessfulHandshake);
        }

        public static Response Event(Command command, int eventDescriptor, IEnumerable<byte> data)
        {
            return new Response(ReturnCode.AsyncEvent, command, eventDescriptor, data.ToArray());
        }

        public override string ToString()
        {
            var result = new StringBuilder("Response(")
                .Append(returnCode);

            if(command.HasValue)
            {
                result
                    .Append(", command: ")
                    .Append(command);
            }

            if(eventDescriptor != null)
            {
                result
                    .Append(", eventDescriptor: ")
                    .Append(eventDescriptor);
            }

            if(data != null)
            {
                result
                    .Append(", data: ")
                    .Append(dataIsText ? Encoding.ASCII.GetString(data) : Misc.PrettyPrintCollectionHex(data));
            }

            return result
                .Append(')')
                .ToString();
        }

        public IEnumerable<byte> GetBytes()
        {
            var response = new List<byte> { (byte)returnCode };

            if(command.HasValue)
            {
                response.Add((byte)command);
            }

            if(eventDescriptor.HasValue)
            {
                response.AddRange(((int)eventDescriptor).AsRawBytes());
            }

            if(data != null && data.Any())
            {
                response.AddRange(checked((uint)data.Length).AsRawBytes());
                response.AddRange(data);
            }

            return response;
        }

        private Response(ReturnCode returnCode)
            : this(returnCode, null, null, null)
        {
        }

        private Response(ReturnCode returnCode, string text)
            : this(returnCode, null, text)
        {
        }

        private Response(ReturnCode returnCode, Command? command, string text)
            : this(returnCode, command, null, Encoding.ASCII.GetBytes(text))
        {
            dataIsText = true;
        }

        private Response(ReturnCode returnCode, Command? command, IEnumerable<byte> data)
            : this(returnCode, command, null, data.ToArray())
        {
        }

        private Response(ReturnCode returnCode, Command? command, int? eventDescriptor = null, byte[] data = null)
        {
            // Command can be null only for FatalError and SuccessfulHandshake
            DebugHelper.Assert(command != null || returnCode == ReturnCode.FatalError || returnCode == ReturnCode.SuccessfulHandshake);
            // eventDescriptor can be null for all return codes but AsyncEvent
            DebugHelper.Assert(eventDescriptor != null || returnCode != ReturnCode.AsyncEvent);

            this.returnCode = returnCode;
            this.command = command;
            this.eventDescriptor = eventDescriptor;
            this.data = data;
            dataIsText = false;
        }

        private readonly bool dataIsText;
        private readonly Command? command;
        private readonly int? eventDescriptor;
        private readonly byte[] data;
        private readonly ReturnCode returnCode;

        // matches the return_code_t enum in tools/external_control_client/lib/renode_api.c
        private enum ReturnCode : byte
        {
            CommandFailed,
            FatalError,
            InvalidCommand,
            SuccessWithData,
            SuccessWithoutData,
            SuccessfulHandshake,
            AsyncEvent,
        }
    }
}
//
// Copyright (c) 2010-2024 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System;
using System.Collections.Generic;
using System.Threading;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Time;
using Antmicro.Renode;
    public class RunFor : BaseCommand, IDisposable
    {
        public RunFor(ExternalControlServer parent)
            : base(parent)
        {
        }

        public void Dispose()
        {
            if(cancellationToken != null)
            {
                parent.Log(LogLevel.Warning, "RunFor disposed while running");
            }
            cancellationToken?.Cancel();
            cancellationToken = null;
            disposed = true;
        }

        public override Response Invoke(List<byte> data)
        {
            if(disposed)
            {
                return Response.CommandFailed(Identifier, "Command is unavailable");
            }

            if(data.Count != 8)
            {
                return Response.CommandFailed(Identifier, "Expected 8 bytes of payload");
            }

            if(cancellationToken != null)
            {
                return Response.CommandFailed(Identifier, "One RunFor command can be running at any given time");
            }

            cancellationToken = new CancellationTokenSource();
            exception = null;
            success = false;

            var microseconds = BitConverter.ToUInt64(data.ToArray(), 0);
            var interval = TimeInterval.FromMicroseconds(microseconds);

            var thread = new Thread(() =>
            {
                try
                {
                    parent.Log(LogLevel.Info, "Executing RunFor({0}) command", interval);
                    EmulationManager.Instance.CurrentEmulation.RunFor(interval);

                    if(cancellationToken.IsCancellationRequested)
                    {
                        return;
                    }

                    success = true;
                }
                catch(Exception e)
                {
                    exception = e;
                }
                cancellationToken?.Cancel();
            })
            {
                IsBackground = true,
                Name = GetType().Name
            };

            thread.Start();
            cancellationToken.Token.WaitHandle.WaitOne();
            cancellationToken = null;

            if(exception != null)
            {
                throw exception;
            }

            if(success)
            {
                return Response.Success(Identifier);
            }
            return Response.CommandFailed(Identifier, "RunFor was interrupted");
        }

        public override Command Identifier => Command.RunFor;

        public override byte Version => 0x0;

        private bool success;
        private Exception exception;
        private CancellationTokenSource cancellationToken;
        private bool disposed;
    }
}
//
// Copyright (c) 2010-2025 Antmicro
// Copyright (c) 2011-2015 Realtime Embedded
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace Antmicro.Renode.Utilities
{
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Sockets;
using Antmicro.Renode;
    public class SimNodusSocketServerProvider : IDisposable
    {
        public SimNodusSocketServerProvider(bool telnetMode = true, bool flushOnConnect = false, string serverName = "")
        {
            queue = new ConcurrentQueue<byte[]>();
            enqueuedEvent = new AutoResetEvent(false);
            this.telnetMode = telnetMode;
            this.flushOnConnect = flushOnConnect;
            this.serverName = serverName;
        }

        public void Start(int port)
        {
            server = SocketsManager.Instance.AcquireSocket(null, AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp, new IPEndPoint(IPAddress.Loopback, port), listeningBacklog: 1, nameAppendix: this.serverName);

            listenerThread = new Thread(ListenerThreadBody)
            {
                IsBackground = true,
                Name = GetType().Name
            };

            stopRequested = false;
            listenerThread.Start();
        }

        public void Stop()
        {
            if(server != null)
            {
                if(!SocketsManager.Instance.TryDropSocket(server))
                {
                    Logger.LogAs(this, LogLevel.Debug, "Failed to drop socket from the manager");
                }
            }
            socket?.Close();
            stopRequested = true;
            cancellationToken?.Cancel();

            var currentThreadId = Thread.CurrentThread.ManagedThreadId;
            if(readerThread?.ManagedThreadId != currentThreadId)
            {
                listenerThread?.Join();
                listenerThread = null;
            }
        }

        public void Dispose()
        {
            Stop();
        }

        public void SendByte(byte b)
        {
            queue.Enqueue(new byte[] { b });
            enqueuedEvent.Set();
        }

        public void Send(byte[] bytes)
        {
            queue.Enqueue(bytes);
            enqueuedEvent.Set();
        }

        public void Send(IEnumerable<byte> bytes)
        {
            Send(bytes.ToArray());
        }

        public void ClearConnectionEvents()
        {
            ConnectionAccepted = null;
            ConnectionClosed = null;
            DataReceived = null;
        }

        public int BufferSize { get; set; } = 1;

        public bool IsAnythingReceiving => DataReceived != null && DataBlockReceived != null;

        public int? Port => (server?.LocalEndPoint as IPEndPoint)?.Port;

        public bool IsStarted => server?.IsBound ?? false;

        public event Action ConnectionClosed;

        public event Action<Stream> ConnectionAccepted;

        public event Action<int> DataReceived;

        public event Action<byte[]> DataBlockReceived;

        private void WriterThreadBody(Stream stream)
        {
            try
            {
                var iacEscapePosition = -1;
                // This thread will poll for bytes constantly for `MaxReadThreadPoolingTimeMs` to assert we have the lowest possible latency while transmiting packet.
                var watch = new Stopwatch();
                while(!cancellationToken.IsCancellationRequested)
                {
                    watch.Start();
                    while(watch.ElapsedMilliseconds < MaxReadThreadPoolingTimeMs)
                    {
                        while(queue.TryDequeue(out var dequeued))
                        {
                            if(!telnetMode || (iacEscapePosition = Array.FindIndex(dequeued, x => x == IACEscape)) == -1)
                            {
                                stream.Write(dequeued, 0, dequeued.Length);
                            }
                            else
                            {
                                // If we're in telnetMode and we discover the IACEscape byte, we should double it
                                stream.Write(dequeued, 0, iacEscapePosition + 1);
                                stream.Write(new byte[] { IACEscape }, 0, 1);
                                var lengthOfRest = dequeued.Length - (iacEscapePosition + 1);
                                if(lengthOfRest > 0)
                                {
                                    stream.Write(dequeued, iacEscapePosition + 1, lengthOfRest);
                                }
                            }
                        }
                    }
                    watch.Reset();
                    enqueuedEvent.WaitOne();
                }
            }
            catch(OperationCanceledException)
            {
            }
            catch(IOException)
            {
            }
            catch(ObjectDisposedException)
            {
            }
            cancellationToken.Cancel();
        }

        private void ReaderThreadBody(Stream stream)
        {
            var size = BufferSize;
            var buffer = new byte[size];

            while(!cancellationToken.IsCancellationRequested)
            {
                if(size != BufferSize)
                {
                    size = BufferSize;
                    buffer = new byte[size];
                }
                try
                {
                    var count = stream.Read(buffer, 0, size);

                    if(count == 0)
                    {
                        break;
                    }

                    if(telnetMode && buffer[0] == IACEscape)
                    {
                        if(iacEscapeSpotted)
                        {
                            // Previous Read ended in IAC
                            // IAC followed by IAC effectively sends 255 as input
                            bytesToIgnore = 0;
                            iacEscapeSpotted = false;
                        }
                        else
                        {
                            // Ignore IAC commands, 3 bytes each (unless followed by another IAC)
                            // TODO: look for iac in the middle of the sequence?
                            // In practice it doesn't seem to happen at all
                            // We explicitly do NOT handle subcommand negotiation, a possible todo for the future
                            bytesToIgnore = IACCommandBytes;

                            if(count == 1)
                            {
                                // We see the IAC code and must ensure the next Read does not yield another one
                                iacEscapeSpotted = true;
                            }
                            else if(buffer[1] == IACEscape)
                            {
                                // We receive a longer data batch starting with two IAC bytes: just skip one byte, leave the other one as is
                                bytesToIgnore = 1;
                            }
                        }
                    }
                    else
                    {
                        // Clear IAC escape mode - we encountered a different character in the package
                        iacEscapeSpotted = false;
                    }
                    var skip = 0;
                    if(bytesToIgnore > 0)
                    {
                        skip = bytesToIgnore > count ? count : bytesToIgnore;
                        bytesToIgnore -= skip;
                        count -= skip;
                    }

                    DataBlockReceived?.Invoke(buffer.Skip(skip).Take(count).ToArray());

                    var dataReceived = DataReceived;
                    if(dataReceived != null)
                    {
                        foreach(var b in buffer.Skip(skip).Take(count))
                        {
                            dataReceived((int)b);
                        }
                    }
                }
                catch(IOException)
                {
                    break;
                }
            }

            Logger.LogAs(this, LogLevel.Debug, "Client disconnected, stream closed.");
            cancellationToken.Cancel();
            enqueuedEvent.Set();
        }

        private void ListenerThreadBody()
        {
            NetworkStream stream;
            while(!stopRequested)
            {
                try
                {
                    socket = server.Accept();
                    stream = new NetworkStream(socket);
                }
                catch(SocketException)
                {
                    break;
                }
                catch(ObjectDisposedException)
                {
                    break;
                }
                try
                {
                    if(telnetMode)
                    {
                        var initBytes = new byte[] {
                            255, 253,   0, // IAC DO    BINARY
                            255, 251,   1, // IAC WILL  ECHO
                            255, 251,   3, // IAC WILL  SUPPRESS_GO_AHEAD
                            255, 252,  34, // IAC WONT  LINEMODE
                        };
                        stream.Write(initBytes, 0, initBytes.Length);
                    }
                }
                catch(OperationCanceledException)
                {
                }
                catch(IOException)
                {
                }
                catch(ObjectDisposedException)
                {
                }

                var connectionAccepted = ConnectionAccepted;
                if(connectionAccepted != null)
                {
                    connectionAccepted(stream);
                }

                if(flushOnConnect)
                {
                    // creating a new queue not to have to lock accesses to it.
                    queue = new ConcurrentQueue<byte[]>();
                }

                cancellationToken = new CancellationTokenSource();
                writerThread = new Thread(() => WriterThreadBody(stream))
                {
                    Name = GetType().Name + "_WriterThread",
                    IsBackground = true
                };

                readerThread = new Thread(() => ReaderThreadBody(stream))
                {
                    Name = GetType().Name + "_ReaderThread",
                    IsBackground = true
                };

                writerThread.Start();
                readerThread.Start();

                writerThread.Join();
                readerThread.Join();

                writerThread = null;
                readerThread = null;

                var connectionClosed = ConnectionClosed;
                if(connectionClosed != null)
                {
                    connectionClosed();
                }
            }
            listenerThread = null;
        }

        private volatile bool stopRequested;
        private Thread listenerThread;
        private Thread readerThread;
        private Thread writerThread;
        private Socket server;
        private Socket socket;
        private int bytesToIgnore;
        private bool iacEscapeSpotted;

        private ConcurrentQueue<byte[]> queue;

        private CancellationTokenSource cancellationToken;
        private readonly AutoResetEvent enqueuedEvent;
        private readonly bool telnetMode;
        private readonly bool flushOnConnect;
        private readonly string serverName;

        private const int IACEscape = 255;
        private const int IACCommandBytes = 3;
        private const int MaxReadThreadPoolingTimeMs = 60;
    }
}
//
// Copyright (c) 2010-2025 Antmicro
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//


namespace SimNodus.Renode.ExternalControl
{
using System;
using System.Collections.Generic;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Utilities;
using Antmicro.Renode;
    public class SystemBus : BaseCommand, IInstanceBasedCommand<IPeripheral>
    {
        public SystemBus(ExternalControlServer parent) : base(parent)
        {
            Instances = new InstanceCollection<IPeripheral>();
        }

        public Response Invoke(IPeripheral instance, List<byte> commandData)
        {
            if(commandData.Count < (int)MinimumPayloadSize)
            {
                return Response.CommandFailed(Identifier, $"Expected at least {MinimumPayloadSize + InstanceBasedCommandHeaderSize} bytes of payload");
            }

            var operation = (Operation)commandData[0];
            var accessWidth = (AccessWidth)commandData[1];
            var commandDataArray = commandData.ToArray();
            var address = BitConverter.ToUInt64(commandDataArray, 2);
            var dataCount = BitConverter.ToUInt32(commandDataArray, 2 + sizeof(ulong));

            if(!ValidateParameters(operation, accessWidth, dataCount, commandData, out var error))
            {
                return error;
            }

            IBusController sysbus;
            IPeripheral context;
            if(instance is IBusController bus)
            {
                sysbus = bus;
                context = null;
            }
            else if(instance is IBusPeripheral busPeripheral)
            {
                sysbus = busPeripheral.GetMachine().GetSystemBus(busPeripheral);
                context = busPeripheral;
            }
            else if(instance is IPeripheral peripheral)
            {
                sysbus = peripheral.GetMachine().SystemBus;
                context = peripheral;
            }
            else
            {
                return Response.CommandFailed(Identifier,
                    $"Invalid instance type: {instance.GetType().Name}, expected {nameof(IPeripheral)} or {nameof(IBusController)}");
            }

            switch(operation)
            {
            case Operation.Read:
                return Response.Success(Identifier, PerformRead(sysbus, context, address, accessWidth, dataCount));
            case Operation.Write:
                var writeData = commandData.GetRange(MinimumPayloadSize, (int)DataCountToByteCount(accessWidth, dataCount));
                PerformWrite(sysbus, context, address, accessWidth, writeData.ToArray());
                return Response.Success(Identifier);
            default:
                throw new Exception("Unreachable");
            }
        }

        public override Response Invoke(List<byte> data) => this.InvokeHandledWithInstance(data);

        public override Command Identifier => Command.SystemBus;

        public override byte Version => 0x0;

        public InstanceCollection<IPeripheral> Instances { get; }

        private bool ValidateParameters(Operation op, AccessWidth width, ulong dataSize, List<byte> commandData, out Response error)
        {
            if(!Enum.IsDefined(typeof(Operation), op))
            {
                error = Response.CommandFailed(Identifier, $"Invalid system bus operation: {op}");
                return false;
            }

            if(!Enum.IsDefined(typeof(AccessWidth), width))
            {
                error = Response.CommandFailed(Identifier, $"Invalid access width: {width}");
                return false;
            }

            var expectedCommandSize = MinimumPayloadSize;
            if(op == Operation.Write)
            {
                expectedCommandSize += (int)DataCountToByteCount(width, dataSize);
            }

            if(commandData.Count != (int)expectedCommandSize)
            {
                error = Response.CommandFailed(Identifier, $"Expected {expectedCommandSize + InstanceBasedCommandHeaderSize} bytes of payload");
                return false;
            }

            error = null;
            return true;
        }

        private ulong DataCountToByteCount(AccessWidth width, ulong size)
        {
            return (width == AccessWidth.MultiByte ? 1 : (ulong)width) * size;
        }

        private byte[] PerformRead(IBusController bus, IPeripheral context, ulong address, AccessWidth width, ulong size)
        {
            var data = new byte[DataCountToByteCount(width, size)];
            if(width == AccessWidth.MultiByte)
            {
                bus.ReadBytes(address, data.Length, data, 0, context: context);
            }
            else
            {
                for(var i = 0; i < data.Length; i += (int)width)
                {
                    switch(width)
                    {
                    case AccessWidth.Byte:
                        data[i] = bus.ReadByte(address + (ulong)i, context);
                        break;
                    case AccessWidth.Word:
                        data.SetBytesFromValue(bus.ReadWord(address + (ulong)i, context), i);
                        break;
                    case AccessWidth.DoubleWord:
                        data.SetBytesFromValue(bus.ReadDoubleWord(address + (ulong)i, context), i);
                        break;
                    case AccessWidth.QuadWord:
                        data.SetBytesFromValue(bus.ReadQuadWord(address + (ulong)i, context), i);
                        break;
                    default:
                        throw new Exception("Unreachable");
                    }
                }
            }
            return data;
        }

        private void PerformWrite(IBusController bus, IPeripheral context, ulong address, AccessWidth width, byte[] data)
        {
            if(width == AccessWidth.MultiByte)
            {
                bus.WriteBytes(data, address, context: context);
            }
            else
            {
                for(var i = 0; i < data.Length; i += (int)width)
                {
                    switch(width)
                    {
                    case AccessWidth.Byte:
                        bus.WriteByte(address + (ulong)i, data[i], context);
                        break;
                    case AccessWidth.Word:
                        bus.WriteWord(address + (ulong)i, BitConverter.ToUInt16(data, i), context);
                        break;
                    case AccessWidth.DoubleWord:
                        bus.WriteDoubleWord(address + (ulong)i, BitConverter.ToUInt32(data, i), context);
                        break;
                    case AccessWidth.QuadWord:
                        bus.WriteQuadWord(address + (ulong)i, BitConverter.ToUInt64(data, i), context);
                        break;
                    default:
                        throw new Exception("Unreachable");
                    }
                }
            }
        }

        private const int InstanceBasedCommandHeaderSize = IInstanceBasedCommandExtensions.HeaderSize;

        private const int MinimumPayloadSize =
            sizeof(Operation) +
            sizeof(AccessWidth) +
            sizeof(ulong) + // Address
            sizeof(uint); // Amount of units to write

        private enum Operation : byte
        {
            Read = 0,
            Write = 1,
        }

        private enum AccessWidth : byte
        {
            MultiByte = 0,
            Byte = 1,
            Word = 2,
            DoubleWord = 4,
            QuadWord = 8,
        }
    }
}
//
// Copyright (c) 2026 Ricardo Kerschbaumer
//
// This file is licensed under the MIT License.
// Full license text is available in 'licenses/MIT.txt'.
//
// SimNodus addition (SN-134): UART access through the external control
// protocol, in the style of the GPIOPort command. Write feeds bytes to the
// peripheral's receiver (WriteChar); RegisterEvent reports every byte the
// peripheral transmits (CharReceived) with Renode's virtual time, during RunFor.

namespace SimNodus.Renode.ExternalControl
{
using System;
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Peripherals.UART;
using Antmicro.Renode;
    public class UART : BaseCommand, IHasEvents, IInstanceBasedCommand<IUART>
    {
        public UART(ExternalControlServer parent)
            : base(parent)
        {
            Instances = new InstanceCollection<IUART>();
        }

        public override Response Invoke(List<byte> data) => this.InvokeHandledWithInstance(data);

        public Response Invoke(IUART instance, List<byte> data)
        {
            if(data.Count < 1)
            {
                return Response.CommandFailed(Identifier, "Expected a UART command");
            }
            switch((UARTCommand)data[0])
            {
            case UARTCommand.Write:
                for(var i = 1; i < data.Count; ++i)
                {
                    instance.WriteChar(data[i]);
                }
                return Response.Success(Identifier);
            case UARTCommand.RegisterEvent:
                if(data.Count != 1 + sizeof(int))
                {
                    return Response.CommandFailed(Identifier, "Expected an event descriptor");
                }
                var descriptor = BitConverter.ToInt32(data.GetRange(1, sizeof(int)).ToArray(), 0);
                instance.CharReceived += value => SendEvent(value, descriptor);
                return Response.Success(Identifier);
            default:
                return Response.CommandFailed(Identifier, "Unexpected UART command");
            }
        }

        public override Command Identifier => Command.UART;
        public override byte Version => 0x0;
        public InstanceCollection<IUART> Instances { get; }
        public event Action<Response> EventReported;

        private void SendEvent(byte value, int descriptor)
        {
            var bytes = new List<byte>(BitConverter.GetBytes(
                (ulong)EmulationManager.Instance.CurrentEmulation.MasterTimeSource.ElapsedVirtualTime.TotalMicroseconds));
            bytes.Add(value);
            EventReported?.Invoke(Response.Event(Identifier, descriptor, bytes));
        }

        private enum UARTCommand : byte
        {
            Write = 0,
            RegisterEvent = 1,
        }
    }
}
