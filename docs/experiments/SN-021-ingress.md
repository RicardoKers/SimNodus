# SN-021 native declaration syntax ingress

Date: 2026-09-14. Status: locally accepted syntax slice; SN-021 remains in progress.
Base main: `5fa212e970e0f363e846d82f81cdb48ec1e9a10c` (PR #21 squash).

## Scope and evidence

[ADR 0054](../decisions/0054-declaration-ingress.md) and the
[contract](../architecture/DECLARATION_INGRESS.md) define acceptance before schema
semantics or graph construction. The pure C++20 API captures caller bytes and
returns immutable original text and source-positioned tokens or structured errors.
It does not accept paths, inspect resources or call engines. Numbers retain their
exact spelling; object keys compare decoded Unicode without case/normalization
folding. Lone escaped surrogates are rejected more strictly than historical Python.

The [audit](evidence/SN-021-ingress-summary.json) records commands, environment,
full CTest output, source/local binary hashes and preserved historical inputs.
Local MSVC 19.51 / Windows SDK 10.0.26100.0 Debug configure/build passed.

- 95 native assertions: ownership, preorder/subtree navigation, scalar types,
  exact large integers/decimal spelling, malformed input, truncated prefixes,
  UTF-8/surrogate boundaries, duplicate keys and depth/byte/breadth limits.
- 209 differential/adversarial cases reconstruct native token trees and compare
  with Python JSON values. Includes every saved schema JSON fixture, deterministic
  generated trees, escaped/raw Unicode, normalization distinctions and explicit
  native/reference policy differences. Invalid-schema fixtures pass syntax only.
- All 20 Windows CTest entries passed, including the unchanged 94 schema tests.
  Native resource tests retained 30 local passes/two unavailable symlink privileges;
  the Python resource suite retained 28 passes/four skips. These are not new engine
  integration results. The unchanged physical tests used ordinary host access.
- 55 historical hashes matched. The previous native audit and all old evidence
  remain unchanged. CMake is extended intentionally. The old native regression
  driver hash predates its PR #21 fixture correction; this audit records its current
  preserved hash separately, without misreporting the older hash as matching.

The initial focused ingress run had 95 assertions and 207 differential cases;
two Unicode-key cases and explicit scalar-kind checks brought the final suite
to 209. Both runs passed. No failed build/test run occurred in this slice.
No binaries or downloaded dependencies are published.
Repository checker: 488 text files passed; `git diff --check` passed.

## Source integration

Source `0dfd974` was pushed on `codex/sn-021-declaration-ingress`.
[PR #22](https://github.com/RicardoKers/SimNodus/pull/22) records final required
Foundation checks and authorized protected-main squash integration. Windows runs
all 20 CTest entries; Linux runs 15, including the same ingress tests and its
existing explicit unsupported-physical-platform behavior. Consult the final
check logs for actual passes/skips; expected job contents are not test results.

## Reproduction and limits

```powershell
cmake -S . -B build/sn021-ingress
cmake --build build/sn021-ingress --config Debug
ctest --test-dir build/sn021-ingress -C Debug --output-on-failure --verbose
python tools/check_repository.py
git diff --check
```

The test probe reads bounded stdin and emits token metadata; it is not a project
format or application IPC decision. The API itself performs no I/O. Input and
depth bound allocations/recursion; allocation failures are structured but were
not fault-injected. No hard wall-time or OS memory quota is claimed. Differential
tests and adversarial cases are bounded evidence, not a general parser proof.

Next implement schema validation over captured syntax before constructing an
immutable source graph. Preserve exact quantities, version/field rejection,
stable IDs, unbound states, references, expansion limits and source locations.
Full native declaration validity, resource compatibility, trust/redistribution,
ELF/boot, saving, compilation and runtime negotiation remain separate gates.
SN-044, MCU/toolchain independence, engine tolerances, PDF/PID fixes and SN-017
Python/GDB fixture ownership remain unchanged.
