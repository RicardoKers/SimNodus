# ADR 0009: bounded offline STM32F103C8 experiment profile

Status: accepted for E-02 evidence; production platform and coupling remain pending.

## Context

The generic pinned STM32F103 platform has oversized memory, an external SVD URL,
no ADC instance, and a fixed RCC tag. SN-019 proved transport and time control only.
E-02 needed target firmware and GPIO evidence without treating model presence as
target coverage.

## Decision

Use owned freestanding firmware and an offline experimental platform with exact
C8 flash/SRAM bounds. Fix CPU throughput and SysTick at 8 MHz for reproducibility.
Name the RCC area `rccRegisterStorage` and treat it only as register storage. Cover
PA0 SysTick output, PA1 input/EXTI1, and firmware acknowledgement on PA2/PA4 through
the verified loopback-only native client.

Accept E-02 for this bounded digital profile. Treat pulls, open-drain, analog mode,
clock propagation, ADC, board wiring, and debugger coordination as unsupported
until their own evidence exists. Use E-02 event observations to define the next
temporal capability profile; do not infer instruction-level timestamps, event
lookahead, or rollback.

## Consequences

- The ELF is independently rebuildable from original MIT-licensed project source.
- Circuit files do not download tools or audit sources and never run this recipe
  automatically.
- Same-time input edges can collapse into one pending interrupt. A callback arrives
  during `RunFor`, but its master-time timestamp is not exact CPU instruction time.
- GPIO electrical behavior requires an explicit electrical boundary rather than
  exposing Renode's Boolean state as a complete pin model.
- The experimental REPL is intentionally incomplete and must not be advertised as
  a general Blue Pill platform.

## Revisit when

E-03 establishes a supported synchronization algorithm, E-04 selects an ADC path,
or a fuller platform provides verified RCC/peripheral behavior without reintroducing
online assets or unreviewed redistribution.
