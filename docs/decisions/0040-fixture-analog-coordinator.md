# ADR 0040: Bounded analog worker coordinator

Date: 2026-09-12. Status: accepted for the SN-017 extraction slice.

## Decision

Extract analog initialization, advance, high/inspection sequencing, raw response
acceptance, RC checks and diagnostics into `FixtureAnalog` in application.
The CLI constructs and owns the existing worker channel and reader. The
noncopyable coordinator borrows nullable pointers to those endpoints, which must
outlive it. It neither creates a backend nor closes an endpoint on destruction.
The CLI retains startup handle validation, global pending guards and joint commit.

Private coordinator state includes initialization, pending advance/read, last
worker result, per-command reply visibility, write counts, RC activation and
legacy advance deadline. Read-only core observations and eligibility predicates
feed ADC preparation and final-inspection activation. Existing adapters still
parse raw replies, enforce their deadlines and manage their pipe handles.

Preserve the original zero-state initialization, increasing sample count on
advance, exact unchanged inspection state and single high transition at
2011375 ns. RC acceptance retains 3.3 V, 1 ms time constant, 10 microvolt tolerance
and 1 ps endpoint agreement. Raw ingress remains exclusive when a reader exists;
legacy normalized analog input retains its prior mode and deadline. Neither
polling nor extraction renews deadlines. Successful analog acceptance does not
publish a joint commit. Unknown commands do not consume arguments.

Explicit abort clears the pending worker gate alongside terminal session abort,
exactly as before; it is not recovery or a new worker command. Per-command reply
visibility is reset before global pending checks. JSON field names and values
are preserved, with a changed object field order for RC diagnostics.

## Evidence and next step

See [evidence](../experiments/evidence/SN-017-analog-coordinator-summary.json) and
[reproduction](../../tests/headless/ANALOG_COORDINATOR.md). Real engine runs
validate the extracted composition. Existing analog command, ingress and exchange
cases cover stale/malformed responses, premature operations, deadlines and
state mutation. A direct C++ pipe check proves coordinator destruction leaves
its borrowed endpoint usable and adapter destruction closes it; premature advance
writes no pipe bytes and does not commit.

The worker adapters, ADC and final-inspection coordinators remain unchanged.
Core/domain gain no third-party types or GUI dependency. This fixture-specific
protocol composition is not a general analog backend or production kernel.
SN-017 remains in progress. Next extract bounded Renode execution coordination
(start/readiness, cancellation and result acceptance) from the CLI, preserving
grant accounting, debug interaction and native transport deadlines. Retain
ADRs 0014/0027/0028 and all unpaced, feedback and peripheral restrictions.
