# Working on SimNodus

Current user instructions take precedence over this file.

## Read first

1. [Current state](docs/planning/CURRENT.md) — short, always up to date.
2. [Requirements](docs/REQUIREMENTS.md), [desktop UI design](docs/architecture/DESKTOP_UX.md)
   and [product architecture](docs/architecture/PRODUCT_ARCHITECTURE.md).
3. [Roadmap](docs/planning/ROADMAP.md) and [backlog](docs/planning/BACKLOG.md).
4. ADRs 0079–0081 in the [decision index](docs/decisions/README.md); older ADRs and
   experiment reports only when touching engine behavior they describe.

## Confirmed direction

- Repository content in English (docs, code, comments). Conversation with the
  owner may be in Portuguese. UI strings in English with a pt-BR translation.
- MIT license for original material; dependencies only under compatible terms
  (no GPL-only libraries linked into the application).
- Windows is the classroom platform; code must also build and run on Linux.
- Owner needs usable releases quickly: 0.1 on 2026-10-30, then every two weeks.

## Technical principles

- C++20. `src/` libraries are Qt-free; Qt 6 Widgets only in `apps/simnodus`.
- No GUI code calls ngspice or Renode directly; use the simulation session API.
- Backends are loaded at run time, never while opening a project. Opening a
  project never executes code or downloads anything.
- Approximate simulation modes are labeled; never claim fidelity that was not
  measured. Backend facts from E-01..E-05 are the reference.
- Stable IDs for components, nets and probes; labels are presentation.

## Workflow

1. Pick the next backlog task of the current phase. Make a coherent, complete
   change (feature + tests + docs), not micro-increments.
2. Verify: build, `ctest`, offscreen GUI smoke test, `python tools/check_repository.py`.
   Never report a test as passed without running it; say what could not be run
   (for example Windows-only behavior) and how it will be checked.
3. Update `CURRENT.md` (replace, do not append a diary), the task status in the
   backlog and the CHANGELOG. Write an ADR only for decisions with lasting
   consequences.
4. Commit with clear messages. Do not invent commit identity; publish only with
   the owner's authorization (granted on 2026-10-14 for this repository).
