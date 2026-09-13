// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
using System;
using System.IO;
using System.Linq;
using System.Threading;
using Antmicro.Renode.Core;
using Antmicro.Renode.Time;
using Antmicro.Renode.Peripherals.CPU;

namespace SimNodus.Experimental.Debugging
{
    public static class JointCancellationProbeExtensions
    {
        public static void StartCancellationProbe(this Emulation emulation, ulong durationUs, string output, ulong cancelAfterUs = 0, int wallPaceMs = 0)
        {
            if(wallPaceMs < 0 || wallPaceMs > 1) { throw new ArgumentOutOfRangeException(nameof(wallPaceMs)); }
            if(worker != null && worker.IsAlive) { throw new InvalidOperationException("Probe still running"); }
            cancellation?.Dispose();
            cancellation = new CancellationTokenSource();
            notified = false;
            lastEnd = 0;
            lastUnused = 0;
            activeWallPaceMs = wallPaceMs;
            var token = cancellation.Token;
            worker = new Thread(() =>
            {
                Action<TimeInterval> signal = null;
                Action<TimeInterval> pace = null;
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
                    if(wallPaceMs != 0)
                    {
                        // Test scheduling only: no engine commands or virtual-time changes.
                        pace = _ => {
                            var delay = Volatile.Read(ref activeWallPaceMs);
                            if(delay != 0) { Thread.Sleep(delay); }
                        };
                        emulation.MasterTimeSource.TimePassed += pace;
                    }
                    var unused = emulation.RunForCancellable(requested, token, () => File.WriteAllText(output + ".ready", "active"));
                    var end = emulation.MasterTimeSource.ElapsedVirtualTime.Ticks;
                    lastEnd = end;
                    lastUnused = unused.Ticks;
                    var sinks = string.Join(",", emulation.MasterTimeSource.Sinks.Select(s => s.TimeHandle.TotalElapsedTime.Ticks.ToString()));
                    var text = "start=" + start + "\nrequested=" + requested.Ticks + "\nend=" + end
                        + "\nreason=" + (unused.Ticks == 0 ? "completed" : "cancelled")
                        + "\nunused=" + unused.Ticks + "\nsinks=" + sinks + "\ncancelled=" + token.IsCancellationRequested;
                    File.WriteAllText(output + ".tmp", text);
                    File.Move(output + ".tmp", output);
                }
                catch(Exception error) { File.WriteAllText(output + ".error", error.ToString()); }
                finally
                {
                    if(signal != null) { emulation.MasterTimeSource.TimePassed -= signal; }
                    if(pace != null) { emulation.MasterTimeSource.TimePassed -= pace; }
                }
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

        public static void ReleaseCancellationProbePacing(this Emulation emulation, string output)
        {
            // Test scheduling only: release host sleep without controlling virtual time.
            Volatile.Write(ref activeWallPaceMs, 0);
            File.WriteAllText(output, "released");
        }

        public static void NotifyCancellationProbe(this Emulation emulation, ulong expectedTicks, string output)
        {
            try
            {
                if(cancellation == null || !cancellation.IsCancellationRequested || !worker.Join(2000)
                    || lastUnused == 0 || lastEnd != expectedTicks || notified)
                {
                    throw new InvalidOperationException("No unnotified cancelled grant at the requested time");
                }
                var cpus = emulation.Machines.SelectMany(m => m.SystemBus.GetCPUs()).OfType<BaseCPU>().ToArray();
                var sinks = emulation.MasterTimeSource.Sinks.ToArray();
                if(cpus.Length != 1 || sinks.Length != 1 || sinks.Any(s => s.TimeHandle.TotalElapsedTime.Ticks != expectedTicks)
                    || emulation.MasterTimeSource.ElapsedVirtualTime.Ticks != expectedTicks)
                {
                    throw new InvalidOperationException("Single-CPU time agreement not established");
                }
                cpus[0].NotifyCancelledRunPause(expectedTicks);
                notified = true;
                File.WriteAllText(output, "notified");
            }
            catch(Exception error) { File.WriteAllText(output, "rejected: " + error.Message); }
        }

        private static bool notified;
        private static int activeWallPaceMs;
        private static ulong lastEnd;
        private static ulong lastUnused;
        private static Thread worker;
        private static CancellationTokenSource cancellation;
    }
}
