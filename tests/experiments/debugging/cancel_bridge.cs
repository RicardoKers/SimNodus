// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
using System;
using System.IO;
using System.Linq;
using System.Threading;
using Antmicro.Renode.Core;
using Antmicro.Renode.Time;

namespace SimNodus.Experimental.Debugging
{
    public static class CancellationProbeExtensions
    {
        public static void StartCancellationProbe(this Emulation emulation, ulong durationUs, string output, ulong cancelAfterUs = 0)
        {
            if(worker != null && worker.IsAlive) { throw new InvalidOperationException("Probe still running"); }
            cancellation?.Dispose();
            cancellation = new CancellationTokenSource();
            var token = cancellation.Token;
            worker = new Thread(() =>
            {
                Action<TimeInterval> signal = null;
                try
                {
                    var start = emulation.MasterTimeSource.ElapsedVirtualTime.Ticks;
                    var requested = TimeInterval.FromMicroseconds(durationUs);
                    if(cancelAfterUs > 0)
                    {
                        var target = start + TimeInterval.FromMicroseconds(cancelAfterUs).Ticks;
                        signal = _ => {
                            // Signal only; no engine control is issued from the callback.
                            if(emulation.MasterTimeSource.ElapsedVirtualTime.Ticks >= target) { cancellation.Cancel(); }
                        };
                        emulation.MasterTimeSource.TimePassed += signal;
                    }
                    var unused = emulation.RunForCancellable(requested, token, () => File.WriteAllText(output + ".ready", "active"));
                    var end = emulation.MasterTimeSource.ElapsedVirtualTime.Ticks;
                    var sinks = string.Join(",", emulation.MasterTimeSource.Sinks.Select(s => s.TimeHandle.TotalElapsedTime.Ticks.ToString()));
                    var text = "start=" + start + "\nrequested=" + requested.Ticks + "\nend=" + end
                        + "\nreason=" + (unused.Ticks == 0 ? "completed" : "cancelled")
                        + "\nunused=" + unused.Ticks + "\nsinks=" + sinks + "\ncancelled=" + token.IsCancellationRequested;
                    File.WriteAllText(output + ".tmp", text);
                    File.Move(output + ".tmp", output);
                }
                catch(Exception error) { File.WriteAllText(output + ".error", error.ToString()); }
                finally { if(signal != null) { emulation.MasterTimeSource.TimePassed -= signal; } }
            }) { IsBackground = true, Name = "SimNodus cancellation probe" };
            worker.Start();
        }

        public static void SampleCancellationProbe(this Emulation emulation, string output)
        {
            File.WriteAllText(output, emulation.MasterTimeSource.ElapsedVirtualTime.Ticks.ToString());
        }

        public static void CancelCancellationProbe(this Emulation emulation)
        {
            cancellation.Cancel();
            if(!worker.Join(2000)) { throw new TimeoutException("Cancellation acknowledgement exceeded two seconds"); }
        }

        private static Thread worker;
        private static CancellationTokenSource cancellation;
    }
}
