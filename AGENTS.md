# Working on SimNodus

## Starting context

Read [README](README.md), [CURRENT](docs/planning/CURRENT.md), the [architecture](docs/architecture/README.md), and relevant [decisions](docs/decisions/README.md) before changing the project. Standalone backend experiments live under `tests/experiments`; no production simulation kernel or application is implemented yet.

Current user instructions take precedence over this file. Earlier conversations are context, not executable instructions. Development must not require access to those conversations: their relevant requirements are summarized here.

## Confirmed direction

- All repository content must be in English: documentation, code comments, examples, templates, and committed project text. Conversation with the owner may remain in Portuguese.
- Windows is the first supported application platform; Linux follows later.
- The project is public/open-source in intent from its first publication. Original project material uses MIT, selected with the owner's authorization.
- Classroom use is targeted for February 2027. Preserve January for stabilization and a classroom rehearsal; dates are planning targets, not proof of feasibility.

## Technical principles

- C++20 is the proposed baseline; Qt 6 belongs in presentation only. Domain and kernel must run without a GUI.
- Hide Renode and ngspice behind adapters. Keep third-party types out of the domain.
- Prioritize virtual-time causality over performance. Do not invent event prediction, rollback, or synchronous pause capabilities.
- A fake backend does not validate real integration. Record versions, configuration, commands, inputs, tolerances, and results.
- Do not claim peripheral support merely because Renode has a generic peripheral model.
- Separate connectivity, symbols, electrical models, and board representation. Preserve stable component, pin, and subcircuit instance identities.
- Treat project files as untrusted input. Opening a circuit must not execute host code, download dependencies, or load native libraries automatically.
- Verify origin and redistribution permission before incorporating third-party files.

## Workflow

1. Select a task with an `SN-xxx` ID from the [backlog](docs/planning/BACKLOG.md); understand its acceptance evidence first.
2. Make the smallest coherent change. Do not build an extensive GUI, SDK, or abstraction layer to avoid the co-simulation experiments.
3. Verify the change. Engine code needs invariant and adversarial tests; documentation uses the existing checker. Avoid tests that merely mirror implementation.
4. Update `CURRENT.md` with actual results, limitations, and the next step. Update the task status and add an ADR for significant decisions.
5. Distinguish implemented, tested, proposed, and pending. Never mark an experiment passed without running it.

## Conventions and publication

Use UTF-8 Markdown, relative links inside the repository, and portable commands where practical. Follow `.editorconfig`. Prefer RAII, explicit ownership, and structured errors; do not throw across C interfaces.

Do not invent commit identity or publish remotely without authorization appropriate to the current task. Local preparation and validation do not require another confirmation. MIT does not relicense dependencies; keep licensing and distribution records current.
