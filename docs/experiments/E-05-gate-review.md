# E-05 bounded capability and gate review

Reviewed: 2026-09-10. **SN-016 is done for the declared bounded experimental
profile in [ADR 0014](../decisions/0014-bounded-cooperative-debugging.md).**
SN-017 is ready; production extraction has not started. General debugging and
fully unpaced interruption are not approved.

The acceptance baseline remains the [original contract](../../tests/experiments/debugging/README.md).
The result combines actual backend/IDE evidence; raw partial-probe flags are
preserved rather than rewritten as full compatibility claims.

## Original acceptance cases

| Case | Measured evidence | Gate result and restriction |
| --- | --- | --- |
| 1. Paused attach at zero | Six final guarded direct-GDB sequences; actual IDE lifecycle and matrix attaches | Passed for one CPU and the owned fixture. |
| 2. GPIO breakpoint and joint boundary | Exact GPIO times, CPU cancellation and persistent RC synchronization in direct GDB and actual IDE | Passed; not arbitrary GPIO event capture. |
| 3. Stable inspection | PC, r0/sp/lr, source, mailbox, time and analog snapshots held for 100 ms | Passed for the side-effect-free fixture allowlist. |
| 4. Instruction step | Expected PC, STEP reason and fixed 2011000 ns boundary | Passed with host grant ownership and analog catch-up. |
| 5. Source step-over | Three actual next commands, expected PCs and increasing firmware.c source lines without entering the helper | Passed with persistent nonzero RC state. |
| 6. ADC continue/readback | Default direct-voltage code 2048 and persistent boundary-sampled code 3541, each under its declared semantics | Passed; physical acquisition/general causal feedback excluded. |
| 7. Actual GDB interrupt | Final direct GDB and actual IDE issue MI interrupt through the cooperative relay; cancellation and analog agreement precede SIGINT | Passed for paced SIGNAL and initially paced arbitration. Raw interruption against the unmodified backend remains rejected. |
| 8. Stopped detach/reconnect | Same-GDB and actual IDE recreation/reconnect with zero, stable digital/analog state | Passed; active disconnect aborts and requires fresh recovery. |
| 9. Coordinated reset | Full engine recreation, zero mailbox/time/analog state; debugger-only reset rejection measured separately | Passed; no in-place reset or rollback. |
| 10. Failure and timeout | Real CPU, analog and debugger loss, live analog response timeout, and the earlier unreachable-stop timeout; no false commit and fresh recovery | Passed for the injected boundaries. Solver nonconvergence and arbitrary failures are not covered. |

Actual CubeIDE launches used its bundled GDB through DSF, without ST-LINK,
OpenOCD or firmware download. The installed product reports 2.2.0; the directory
name 2.1.1 is not the measured version. Loopback ownership and listener cleanup
were checked. The default guarded lifecycle retains exact reference stop/circuit
records after the final startup change.

## Final evidence and coherence

- [Final gate evidence](evidence/E-05-final-gate-summary.json): six guarded
  direct-GDB step/pause/lifecycle sequences with six retained actual MI interrupts,
  41.5932–91.5682 ms acknowledgement, ADC 3541 and complete cleanup; three IDE
  release-note-suppressed controls, including extended and default lifecycle.
- [Extended matrix](evidence/E-05-steps-fault-matrix-summary.json): 35 sessions on
  matching sources/binaries. Three arbitration fault/recovery pairs per fault,
  four continuously paced fault/recovery controls and three lifecycle/sequence
  controls all passed.
- [Direct shared-bridge regression](evidence/E-05-steps-fault-plain-regression-summary.json):
  six host-requested pause/lifecycle sequences. This older request path is
  explicitly distinct from the final actual MI/relay path.
- [Historical report](E-05-results.md): original GDB/IDE evidence, command rejection,
  source-build equivalence, cancellation/notification, persistent analog and
  boundary correction. The 14 current relay/accounting tests also passed.

The final MI check shares the engine, guard, coordinator and bridge inputs used
by the successful matrix. The direct runner adds relay routing without changing
the matrix's Analog worker implementation. The IDE startup change only seeds
the documentation preference and verifies it; final lifecycle controls validate
that change. All hashes and raw paths remain in the evidence. Do not claim that
every historic subprofile ran on one final identical source revision.

## Preserved restrictions and failures

The final arbitration matrix uses 1 ms callback pacing until retained MI
interruption and a read-only pre-ADC observation, then releases pacing. Host
polling is 1 ms, and the injected delay is at least 50 ms. Fully unpaced
injection remains unreliable; three failed candidates remain failed. A fourth
matrix failed on transient CPU-result access. The bounded retry correction has
controlled unit evidence; the succeeding real matrix required no natural retry.

Retain 1 ps analog endpoint agreement, 10 microvolt RC tolerance, two-second
normal pause/failure handling, and the existing five-second total analog-timeout
case limit. CPU acknowledgement never substitutes for a joint commit. Reset is
full recreation; failures never recover an abandoned grant.

Arbitrary firmware/circuits, deterministic wall-triggered pause time, physical
ADC acquisition, solver nonconvergence recovery, mouse-driven IDE behavior,
multicore systems, authentication and a production kernel remain unvalidated.
The historical Eclipse exception remains a known issue; current automatic
closure and zero exits were measured. No failed record is relabelled.

## Next work

Begin SN-017 with a small headless fixture runner and explicit backend contracts
that preserve granted, observed, CPU-acknowledged and jointly committed state.
Use real-backend regression as the acceptance evidence. Keep Qt out of the
kernel, third-party types behind adapters, and general unpaced capability
disabled. Do not build a broad GUI or SDK before the minimal extraction works.
Keep the January stabilization and February 2027 teaching targets unchanged.
