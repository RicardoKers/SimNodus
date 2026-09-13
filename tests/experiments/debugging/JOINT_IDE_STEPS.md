# Persistent joint instruction-step and source-step-over sequence

Use a separate --joint-steps option in the paced joint profile. Keep one real
persistent circuit across GPIO, instruction-step, step_target and three actual
MI next commands, asynchronous pause, ADC and committed-result stops. Each
new CPU authorization must be cancelled and accounted before the circuit follows
the observed endpoint. Require clock agreement within 1 ps, analytical RC error
within 10 microvolts and stable CPU/mailbox/analog inspection for 100 ms.

Require fixed stop times 2010875, 2011000, 2011375, 2012125, 2012250, 2012500,
2012875, then a variable actual SIGNAL pause strictly later than 2012875 and
before 4010750, followed by ADC at 4010750 and commit at 4027000 ns. The step
commands must yield STEP, expected PCs, increasing firmware.c source lines in
step_target and no debugger stop inside step_helper. Apply GPIO high at 2011375 ns and
transfer ADC exactly once at its boundary; require firmware code 3541.

The progress handshake must observe time beyond the last accepted boundary,
not merely beyond the earlier GPIO edge. This prevents an interrupt before the
new grant has made progress. Keep only the asynchronous interval wall-paced.

After a fresh development run, run three --joint-steps --joint-lifecycle sessions:
two complete ten-checkpoint sequences per IDE with recreation, detach/reattach
and persistent zero-state checks between them. Require automatic IDE closure,
zero engine exits and listener cleanup. Regress the earlier joint sequence and
default lifecycle. Other joint fault/race options are excluded from this new
step option until specifically integrated. Existing scoped fault evidence is
preserved; this result alone does not close full E-05 approval.

The final repetitions also compare r0, sp and lr before and after the 100 ms
inspection interval at every checkpoint, in addition to PC, mailbox, virtual
time and the persistent analog snapshot. Source-frame inspection requires one
top frame in firmware.c and strictly increasing lines for the step_target/next
sequence. The initial development run predates these explicit register checks;
retain it separately from the matching-source lifecycle repetitions.

The first lifecycle candidate rejected decimal r0 value `1` because a helper
expected hexadecimal text. Keep that failed test. Register stability checks
now compare the actual GDB representations as strings, matching the earlier
default inspection path, without changing simulated registers. Final repetitions
use fresh directories and matching source hashes after this correction.

## Verified combined lifecycle result (2026-09-09)

Three actual IDE sessions (joint-steps-lifecycle-02 through 04) passed with
matching source and binary hashes. Each contained two complete persistent
ten-checkpoint sequences, separated by coordinated engine recreation and actual
DSF detach/reattach at zero. All six sequences preserved the nine fixed firmware
boundaries, observed one later asynchronous SIGNAL pause, advanced through the
expected source frames, and produced ADC code 3541. Register, PC, mailbox, CPU
time and analog inspections remained stable. The circuit retained nonzero RC
state through the source steps after GPIO high.

Host pause acknowledgements ranged from 45.8441 to 86.6593 ms; every IDE also
enforced the two-second request limit. All IDEs accepted closure and exited
zero, and old/new engine and relay listeners were removed. The fresh earlier
five-checkpoint joint control also passed. The failed first lifecycle candidate
and initial single-sequence development run remain separate from this evidence.

The default guarded lifecycle also retained the exact nine stop/circuit records
and reset/reconnect cleanup. [Consolidated evidence](../../../docs/experiments/evidence/E-05-joint-steps-summary.json)
records all six sequences and both controls. Plain-GDB reconciliation and fault/
arbitration integration with the extended step option remain separate checks;
full E-05 approval is not inferred from these successful IDE lifecycle runs.

The subsequent [extended fault/arbitration integration](JOINT_STEPS_FAULTS.md)
adds separately labelled combinations to this option. Earlier exclusions above
describe the original step-only experiment, not the current CLI policy.
