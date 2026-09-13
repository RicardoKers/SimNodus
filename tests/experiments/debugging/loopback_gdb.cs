// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Experiment-only bootstrap for the exact audited Renode 1.16.1 transport.
using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Threading;

using Antmicro.Renode.Core;
using Antmicro.Renode.Sockets;
using Antmicro.Renode.Utilities;

namespace SimNodus.Experimental.Debugging
{
    public static class LoopbackGdbServerExtensions
    {
        public static void StartLoopbackGdbServer(this Machine machine, int port)
        {
            if(port < 1 || port > 65535)
            {
                throw new ArgumentOutOfRangeException(nameof(port));
            }

            var terminal = new SocketServerProvider(telnetMode: false, serverName: "SimNodus-GDB");
            Action<Stream> attach = null;
            attach = stream =>
            {
                terminal.ConnectionAccepted -= attach;
                machine.StartGdbServer(terminal);
            };
            terminal.ConnectionAccepted += attach;

            try
            {
                StartAuditedLoopbackTransport(terminal, port);
            }
            catch
            {
                terminal.Dispose();
                throw;
            }
        }

        private static void StartAuditedLoopbackTransport(SocketServerProvider terminal, int port)
        {
            var type = typeof(SocketServerProvider);
            var serverField = RequiredField(type, "server", typeof(Socket));
            var listenerThreadField = RequiredField(type, "listenerThread", typeof(Thread));
            var stopRequestedField = RequiredField(type, "stopRequested", typeof(bool));
            var listenerBody = type.GetMethod("ListenerThreadBody", BindingFlags.Instance | BindingFlags.NonPublic);
            if(listenerBody == null || listenerBody.ReturnType != typeof(void)
                || listenerBody.GetParameters().Length != 0)
            {
                throw new InvalidOperationException("Audited SocketServerProvider listener shape changed");
            }

            var server = SocketsManager.Instance.AcquireSocket(
                owner: null,
                addressFamily: AddressFamily.InterNetwork,
                socketType: SocketType.Stream,
                protocolType: ProtocolType.Tcp,
                endpoint: new IPEndPoint(IPAddress.Loopback, port),
                listeningBacklog: 1,
                nameAppendix: "SimNodus-GDB");

            var listenerThread = new Thread(() => listenerBody.Invoke(terminal, null))
            {
                IsBackground = true,
                Name = type.Name
            };
            stopRequestedField.SetValue(terminal, false);
            serverField.SetValue(terminal, server);
            listenerThreadField.SetValue(terminal, listenerThread);
            listenerThread.Start();
        }

        private static FieldInfo RequiredField(Type type, string name, Type fieldType)
        {
            var field = type.GetField(name, BindingFlags.Instance | BindingFlags.NonPublic);
            if(field == null || field.FieldType != fieldType)
            {
                throw new InvalidOperationException($"Audited SocketServerProvider field changed: {name}");
            }
            return field;
        }
    }
}
