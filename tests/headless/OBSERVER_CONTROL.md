# SN-018 observer control contract

Predeclared before execution, 2026-09-12. Retain the exact
[baseline profile and tolerances](BASELINE.md), ADRs 0014/0043 and the host-owned
preparation/GDB/fixture sequence. No production or capability change is made.

Run `python tests/headless/measure_observer.py --output build/sn018/observer-01`.
Use a fresh directory. Verify the baseline manifest inputs and all historical
baseline raw hashes before starting. Freeze the new script, this contract,
commands, host metadata and preserved evidence hashes in a new manifest.

Execute six sequential batches in three adjacent pairs: **U,S / S,U / U,S**.
U disables process-tree/memory sampling; S uses the unchanged baseline sampler.
Each batch uses the original three initial/recreated pairs, six fresh sessions:
18 sessions per condition, 36 total. No warm-up, randomization, cache clearing,
outlier removal or retries. Stop and preserve a failed batch. Alternate pair
order to reduce order bias; three pairs on one uncontrolled host cannot remove
thermal, cache, background-load or time-of-day confounding.

Both conditions use the same outer process poll followed by 100 ms sleep. Only S
calls the memory sampler and retains its rows. Thus this tests incremental
memory-observer work, not removal of GDB, logging, harness checks or polling.
The S sampling call extends the polling period just as in the initial baseline.
Batch wall time includes exit-detection delay and all harness activity. Record
sampler-call wall duration separately; it is an elapsed observer cost, not CPU
time or causal simulation slowdown. Memory in U is **unmeasured**, never zero.

For each pair report S minus U batch wall duration and 100*(S/U-1), and the
difference between each batch's median pause latency. Report all session pause
values and min/median/max/sample standard deviation by condition. Sessions are
nested within batches; no significance test, p99, equivalence threshold or
universal overhead percentage is justified. Compare historical baseline only
as context, not as the contemporaneous control.

Reuse the existing pause timer (before MI interrupt through stopped readback,
excluding subsequent stability wait/commit), fixed-state comparison with cycle
27, analytical RC and endpoint error calculations. Preserve 10 microvolts,
1 ps and 2 s limits; nine fixed checkpoints, final 4027000 ns and ADC 3541.
Variable pause virtual boundary is not a fixed checkpoint. Record unchanged
inputs after the series and retain previous evidence byte-for-byte.

Interpretation is descriptive: consistent positive paired differences would
suggest observer contribution on this host; mixed signs or large variability
leave slowdown unresolved. Either result completes this control slice, not
SN-018 as a whole. Next assess bounded baseline coverage rather than extending
MCU/circuit/debug support. PDF suppression, PID retry and ADRs 0027/0028 remain.
