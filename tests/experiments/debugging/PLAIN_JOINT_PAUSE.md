# Direct GDB persistent asynchronous host pause

Predeclared 2026-09-09 for SN-016. Extend the nine fixed step checkpoints with
one asynchronous host-requested pause after the final source step (2012875 ns)
and strictly before the ADC marker (4010750 ns). This runner connects directly
to GDB: it issues no raw interrupt and does not validate MI interrupt interception.
The existing guarded IDE experiment owns that separate command path.

Use the measured cooperative bridge with 1 ms wall pacing per time callback
only during the asynchronous grant. Observe progress beyond the last step,
request host cancellation, and verify start/end/unused time and every sink.
Advance the same analog circuit to that endpoint, requiring 1 ps time agreement
and 10 microvolt analytical accuracy before requesting verified GDB notification.
Require SIGINT, total request-to-GDB acknowledgement within two seconds, and
100 ms stable CPU/register/mailbox/analog inspection. A CPU cancellation result
alone is not a joint checkpoint. A breakpoint collision fails this bounded
profile; no general unpaced or arbitration support is inferred.

Resume with new grants to the unchanged ADC and final firmware stops, requiring
code 3541. Run three repetitions with lifecycle enabled: six sequences, each
with nine fixed checkpoints and one variable pause. Recreate both engines and
check actual same-GDB detach/reattach at zero. Compare fixed CPU/register/GPIO
states exactly and analog voltages within 10 microvolts; variable pause time,
solver sample counts and the following grant's start/unused time need not match.
Require normal process exits and listener removal. Repeat the original
four-stop regression in a fresh output directory. Preserve failed candidates.

```powershell
python tests/experiments/debugging/joint_persistent.py --renode build/sn016/source-notification --ide C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE --native build/sn016/persistent-native/Debug --steps --lifecycle --pause --output build/sn016/<fresh-directory>
```

Extended IDE fault/arbitration integration and the final capability decision
remain separate work. This protocol does not open SN-017.

## Measured result

Three lifecycle repetitions passed in `build/sn016/plain-joint-pause-01`:
54 fixed checkpoints plus six asynchronous pauses. Pause times were 2014000,
2014000, 2013000, 2013000, 2013000 and 2013000 ns; request-to-GDB acknowledgement
ranged from 42.7100 to 54.3480 ms. All six sequences reached ADC code 3541 with
stable inspection, zero process exits and removed Renode listeners. The three
recreated sessions passed same-GDB detach/reattach at zero.

Across 324 fixed boundary comparisons with the six previous extended IDE
sequences, GPIO/time agreed and maximum analog difference was 1.7969e-11 V.
Three four-stop regressions in `build/sn016/plain-joint-pause-control-01` matched
`plain-joint-lifecycle-control-01` checkpoint records exactly. See
[evidence](../../../docs/experiments/evidence/E-05-plain-joint-pause-summary.json)
for full checkpoints, pause accounting, source/binary hashes and limitations.
