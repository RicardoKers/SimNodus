# ADR 0012: use a focused STM32F103 ADC experiment extension

Date: 2026-09-01. Status: accepted for E-04 evidence; production integration pending.

## Context

The selected offline STM32F103C8 platform has no ADC instance. Renode infrastructure commit `add012af003a0f620d3da52828262676f374d121` contains a generic `STM32_ADC`, but its measured contract is unsuitable for the intended F103 path: regular software start is at bit 30 instead of F103 bit 22, external samples are queued raw codes, and the class does not implement Renode's integer-microvolt `IADC` interface.

Using that model unchanged would make owned F103 firmware address the wrong register field. Converting voltage to a code in the host would also move quantization outside the peripheral boundary and make future electrical integration prone to double quantization.

## Decision

Use an owned `SimNodusSTM32F103ADC` extension for E-04. Keep it in the experiment layer and implement only the audited subset needed to test the first ADC lesson and E-05 breakpoint:

- F103 ADC1 base/register fields for one regular software-triggered rank;
- 16 external channels through the pinned integer-microvolt `IADC` contract;
- fixed 3.3 V reference, 12-bit single quantization, saturation, and right-aligned data;
- fixed 1 MHz ADC clock, the eight sampling-time selections, start-time voltage capture, EOC, and DR clear behavior;
- explicit rejection or nonimplementation of every mode outside that subset.

Do not present the extension as an upstream Renode model, a production adapter, or complete STM32F103/Blue Pill support. Keep direct known-schedule voltage injection separate from ngspice electrical coupling.

## Alternatives

- The pinned generic model was rejected because its register and external-input contracts do not match this experiment.
- Host-side conversion to a raw code was rejected because the host would own peripheral quantization and obscure the voltage boundary.
- A complete STM32F103 ADC implementation was deferred because E-04 needs a narrow evidence gate, and unsupported modes require separate firmware and electrical tests.
- Waiting for an upstream model was rejected because the focused extension is small, source-audited, and sufficient to test the required path without claiming broader support.

## Consequences

- Firmware can exercise an F103-compatible ADC read while the host supplies voltage once in explicit units.
- The extension must remain isolated behind the future Renode adapter; third-party and experiment types must not enter the domain.
- Any production extraction must preserve the measured conversion contract and structured unsupported-feature diagnostics.
- Electrical coupling still needs an explicit pin/acquisition model. E-04 does not widen E-03's causal profile.
- A Renode or ST source revision requires rerunning the source audit and complete E-04 profile.

## Revisit criteria

Revisit when upstream Renode adds a verified F103-compatible `IADC` model, a lesson needs an unsupported ADC mode, dynamic VDDA/source impedance becomes required, ADC2 is selected, electrical ADC coupling is implemented, or the pinned backend revisions change. A replacement must rerun units, boundaries, saturation, channel mapping, ramp, timing, sampling instant, disabled-start, invalid-channel, process, and repetition gates.
