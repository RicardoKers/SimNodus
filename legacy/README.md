# Legacy product line (archived 2026-10-14)

This folder holds the product code of the first development line
(2026-08-31 → 2026-10-06): the bounded headless kernel experiments, the
project/resource validation libraries, the Windows managed store and the
fixed-RC desktop slice. It was moved here by SN-101 under
[ADR 0079](../docs/decisions/0079-product-rebuild-and-lightweight-process.md)
and is **not part of the default build**.

The backend experiments E-01..E-05 stay in [tests/experiments](../tests/experiments)
and remain the reference for engine behavior. Their history is documented in
[docs/archive/2026-10-legacy](../docs/archive/2026-10-legacy/CURRENT.md).

## Building it

The tree still builds and tests on its own (verified on Windows with MSVC
19.51 on 2026-10-14: 67/67 tests passed):

```powershell
cmake -S legacy -B build/legacy -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/legacy
ctest --test-dir build/legacy
```

Use a Visual Studio developer shell. `legacy/LICENSE` is a copy of the root
license because the legacy fixtures reference it as a project resource. Tests
write scratch output to `legacy/build/` (ignored).

New code must not depend on anything in this folder; copy and adapt instead.
