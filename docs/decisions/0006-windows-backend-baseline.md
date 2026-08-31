# ADR 0006: Pin a Windows experiment baseline

Date: 2026-08-31. Status: accepted for standalone experiments; production packaging and integration unproven.

## Context

SN-010 found exact official packages and an existing native C++/ARM toolchain. The current Renode online documentation differs from its stable release API. Real setup checks also found a Windows client build failure and ngspice startup/dependency details that a version list alone would conceal.

## Decision

Pin Renode 1.16.1's Windows portable .NET package, matching client/platform source, and ngspice 47's VS x64 DLL, matching console dependencies, and source archive. Keep SHA-256 records and an explicit opt-in setup recipe. Do not download them in the root CMake build or vendor them into the public repository.

Use the installed native MSVC C++20 toolchain for the first host probe and the existing CubeIDE GNU Tools for STM32 for later firmware experiments. Qt remains deferred. Resolve the official client's native Windows compatibility as SN-019, retaining the upstream protocol and time units rather than guessing an interface.

Use an owned ngspice initialization file after the documented pre-init call crashed the selected binary. This is an experiment setup decision, not a proven lifecycle design for the future adapter.

## Consequences

E-01 has a reproducible real DLL startup baseline. E-02 still needs native client adaptation, an offline C8 platform, and owned firmware. Optional code models remain unloaded. No co-simulation, ADC, timing, or debugger capability follows from these checks. Complete transitive license and runtime security reviews before distributing binaries; the development pin is not release approval.

## Revisit when

A standalone experiment exposes a blocking API defect, a minimal upstream fix/new release removes a limitation, or a runtime security requirement demands an update. Change the pin with measured evidence, update hashes and reports, and rerun affected experiments. See [SN-010 results](../experiments/SN-010-results.md).
