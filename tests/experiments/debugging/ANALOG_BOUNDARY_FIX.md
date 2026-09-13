# Persistent analog boundary rounding correction

## Failure and source audit

The retained joint-race-06 failure and three direct worker reproductions missed
the requested 3140000 ns checkpoint. Instrumentation showed the integration
samples included exactly the requested double (0.00314 s), but the foreground
stop happened at 0.0031400124999999999 s, 12.5 ns later. The original binary and
source are preserved in build/sn016/analog-boundary-fix.

The pinned ngspice 47 frontend/breakp.c implements >= as a raw double comparison
but equality via AlmostEqualUlps(..., 3). Its frontend/parser/numparse.c builds
the decimal fraction with a numerator times a power of ten. Reconstructing that
arithmetic for 0.00314 yields the adjacent higher double; this explains why the
integration sample can precede the parsed >= threshold. This is source-based
numerical reconstruction, not a trace of the runtime parser internals.

## Correction and unchanged acceptance criteria

Keep ngSpice_SetBkpt at the requested boundary and change the joint worker's
foreground condition to `stop when time = <target>`, using ngspice's three-ULP
equality. Independently require the actual final sample to agree within 1 ps
before publishing it. Do not round or relabel an overshot state. Retain a precise
failure diagnostic with requested, final and nearest observed times.

The correction applies to joint_circuit.cpp. The ngspice DLL, CPU backend,
firmware, timestep, RC tolerance and IDE deadlines are unchanged. Revalidate
the equality behavior if the pinned dependency changes.

## Regression protocol

Run `joint_boundaries.py --output <fresh-directory>`. The real worker exercises
70 distinct intermediate boundaries, including the failed time, adjacent 1 ns
and 125 ns offsets and a grid across the GPIO/ADC interval, plus two repeats of
3140000 ns. Every case advances through the original GPIO stops, applies GPIO
high, inspects stable state, reaches its intermediate boundary, then resumes to
the ADC and committed-result times. Require 1 ps agreement, 10 microvolt RC
accuracy, monotonic samples, unchanged inspect results and ADC code 3541.

Follow with three actual unpaced IDE arbitration sessions and paced pause,
already-observed breakpoint, and delayed-host race controls. Keep the previous
failed evidence. This is bounded fixture validation, not the complete E-05 gate.

## Verified result (2026-09-09)

All 72 direct-worker cases passed. Maximum absolute time error was
8.673617379884035e-19 s, below the unchanged 1e-12 s limit. All six actual IDE
sessions passed with matching source/binary hashes, stable inspection, one ADC
transfer per sequence, result code 3541, accepted IDE closure, zero process
exits and removed owned listeners. The regression binary hash matches the IDE
worker hash. [Consolidated evidence](../../../docs/experiments/evidence/E-05-analog-boundary-fix-summary.json)
includes the retained diagnostic failure and source-audit hashes.

This resolves the reproduced 3140000 ns joint-worker failure. Earlier failed
runs remain historical failures. Arbitrary circuits, arbitration fault teardown
and the complete E-05 gate remain unvalidated.
