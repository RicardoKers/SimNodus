# ADR 0039: Bounded ADC application coordinator

Date: 2026-09-12. Status: accepted for the SN-017 extraction slice.

## Decision

Extract ADC preparation, application, result polling, legacy confirmation and
diagnostics into `FixtureAdc` in `src/application/fixture_adc.hpp`. It privately
owns the prepared input, pending state, confirmation count, original deadline
and the existing `renode::AdcProcess` instance. The CLI validates startup helper
configuration and passes it at construction. It supplies a core analog observation
and current preparation eligibility, retaining worker and joint-session ownership.

The coordinator is noncopyable through its unique process ownership. Destruction
retains the adapter's RAII cleanup, including EOF exit. Explicit session abort
calls coordinator cancellation. Read-only accessors supply ADC facts to the final
inspection coordinator; callers cannot modify transfer state. Unknown commands
remain unhandled without consuming arguments.

Keep the existing protocol, preparation at 4010750 ns, one transfer, rounded
microvolts, expected firmware code and 1900 ms preparation deadline. Starting or
polling the helper does not renew that deadline. Native-helper mode still rejects
host-normalized confirmation. Only a valid completed helper reply confirms a
transfer; confirmation does not publish a joint commit. Preparation and polling
errors retain their previous terminal session behavior.

This is application-level composition of existing core checks and an adapter,
not a new ADC model or generic backend API. Core/domain gain no third-party types.
The helper launch/result/cleanup implementation and final inspection coordinator
are unchanged. No native library loading or extra host execution is introduced.

## Validation and next step

See [evidence](../experiments/evidence/SN-017-adc-coordinator-summary.json) and
[reproduction](../../tests/headless/ADC_COORDINATOR.md). Existing process cases
cover timeout, shared deadline, malformed/replayed results, descendants, runner
loss and cleanup. Direct coordinator checks cover isolated instances, single
confirmation, pending policy, denied preparation and argument-preserving delegation.
Real Renode/ngspice/GDB runs validate the extracted composition separately.

SN-017 remains in progress. Next extract bounded analog worker coordination
(initialization, catch-up and high/inspection sequencing) from the CLI, preserving
raw reply ingress, RC checks, deadlines and backend ownership. Keep ADRs
0014/0027/0028, host GDB ownership and all capability restrictions. No physical
ADC acquisition, general causal feedback or unpaced debugging is implied.
