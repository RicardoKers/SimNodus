# Current state

Updated: 2026-10-14. Keep this file short: present state, last results, next
steps. History of the first development line (2026-08-31 → 2026-10-06) is in
[archive/2026-10-legacy/CURRENT.md](../archive/2026-10-legacy/CURRENT.md).

## Where we are

- **Phase 0 — Reset** of the [roadmap](ROADMAP.md). The product layer is being
  rebuilt ([ADR 0079](../decisions/0079-product-rebuild-and-lightweight-process.md)).
- New [requirements](../REQUIREMENTS.md), [UI design](../architecture/DESKTOP_UX.md)
  and [product architecture](../architecture/PRODUCT_ARCHITECTURE.md) are written;
  [backlog](BACKLOG.md) restarted at SN-100.
- No new application code yet. The previous bounded editor (fixed RC fixture)
  still sits in `apps/desktop` until SN-101 moves it to `legacy/`.

## Verified on 2026-10-14 (Linux container)

- Snapshot integrity: Git `main` at `e1aab64` (PR96) plus the 12 local SN-045
  documentation files reproduce all 1059 files of the handoff manifest by SHA-256.
- Previous foundation: CMake build without warnings (GCC 13), ctest 56/56,
  pytest 124 passed / 29 skipped (Windows-only), repository checker passed.
- The previous desktop app compiles against Qt 6.4 on Linux but cannot open
  projects there (Windows-only I/O).
- Tooling available for development and tests in the Linux environment: Qt 6.4,
  ngspice 42 shared library, Renode 1.16.1 Linux portable (downloadable),
  `arm-none-eabi-gcc` 13.2.
- ngspice memory measurement (input to ADR 0080): ~48 MB resident for 2 s of
  simulated RC transient at 1 µs maximum step.

## Next steps

1. SN-101 — move previous product code to `legacy/`, new CMake layout and CI.
2. SN-102 — application skeleton with the three-column window and analyzer window.
3. SN-103/SN-104 — engine spikes (continuous run memory/pacing; Windows DLL).
4. Then phase 1 tasks SN-110..SN-123 toward release 0.1 on 2026-10-30.

## Known constraints

- Publishing to GitHub needs write access from the working session or the
  owner pushing a delivered bundle.
- Windows builds are produced by CI or by the owner's local Qt kit; the
  development environment is Linux.
