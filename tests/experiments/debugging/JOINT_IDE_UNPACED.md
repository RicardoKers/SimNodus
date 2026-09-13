# Unpaced joint IDE interruption characterization

Repeat three fresh actual-IDE sessions with --joint-wall-pace-ms 0. Keep the
same firmware, 100000 us active grant, read-only progress handshake, strict
2011375 < pause < 4010750 ns interval, two-second acknowledgement deadline,
1 ps clock tolerance, 10 microvolt RC tolerance and ADC code 3541. No bridge
sleep callback is subscribed when pacing is zero. Do not remove the ADC
breakpoint or enlarge the timing window to obtain a pass.

Record sampled CPU times with host elapsed times from active-grant request,
the intercepted interrupt timing, and any candidate cancellation outcome before
validating it. These host observations include command and sampling latency;
they are not exact CPU instruction or network-latency timestamps.

A failed candidate remains failed. Preserve raw logs, rejected outcome and
cleanup evidence. Never publish a third joint checkpoint or verified pause
notification if the CPU escapes the declared interval. Run a fresh paced control
and the default lifecycle regression after modifying the shared driver. This
characterizes the existing fixture; success alone would not establish arbitrary
workload support or exact physical ADC aperture timing.

```powershell
python tests/experiments/debugging/cubeide.py --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --output build/sn016/joint-unpaced-next --profile joint --guarded --joint-wall-pace-ms 0 --experimental-renode build/sn016/source-notification
```

## Observed result (2026-09-09)

One of three unpaced sessions passed. The first paused at 3838000 ns, produced
stable joint inspection, and resumed through ADC code 3541. The other two
cancelled at exactly 4010750 ns, the ADC breakpoint, outside the strictly earlier
pause interval. Their cancellation accounting and sink times agreed, but the
candidate endpoints were rejected before analog advancement or joint pause
notification. Both retained only two accepted checkpoints and removed the
owned relay/backend listeners. They remain failed runs, not supported pause
cases. The experiment host aborted those sessions; orderly IDE closure is not
claimed for rejected candidates.

The progress observations first saw execution at 2678000, 2524000 and 2538000 ns.
Interrupt interception followed approximately 16.6, 25.6 and 23.4 ms later in
host time. This includes the progress-file/IDE/DSF/relay path and does not isolate
one component's latency. A successful run therefore does not establish reliable
interruption inside this short interval. The paced control passed with the same
source/binary hashes, and the default lifecycle retained exact reference stops,
circuits and reset/reconnect cleanup.

[Consolidated evidence](../../../docs/experiments/evidence/E-05-joint-ide-unpaced-summary.json)
retains all three outcomes. Unpaced support remains unapproved.

## Next bounded investigation

Investigate arbitration when a requested pause races with an already-hit fixture
breakpoint. A candidate must distinguish the observed breakpoint from an actual
SIGNAL stop, synchronize persistent analog state to a verified CPU endpoint, and
perform the ADC boundary transfer exactly once before resuming. It must not emit
a fabricated SIGINT or simply relax this experiment's strict interval check.
Predeclare that as a separate profile, retaining these failures. Include both
pause-before-breakpoint and breakpoint-before-pause cases, exact accounting,
state stability, resumed ADC result, fault teardown and normal-profile regression.
No production capability or changed ownership rule is approved by this proposal.
