# Headless operation and automation

Status: accepted architectural direction under [ADR 0055](../decisions/0055-automation-first-headless.md), recorded by SN-045. Since 2026-10-25 [ADR 0104](../decisions/0104-external-control-and-io.md) selects the concrete interface: JSON commands in `src/automation`, used by `simnodus-cli` (SN-215), the desktop window (SN-216) and its MCP server (SN-217); the commands are documented in [docs/user/AUTOMATION.md](../user/AUTOMATION.md). The rest of this page is the original direction.

## Purpose and application boundary

**The SimNodus headless interface should be automation-first and machine-readable, suitable for CI, scripting, external tools and AI agents.**

Advanced users, automated regression systems and external agents are intended consumers from the start. Reproducible backend experiments remain a first use of the existing `apps/cli` destination; this direction does not introduce a competing CLI.

```text
            Shared Application / Automation capabilities
                              |
              +---------------+----------------+
              |               |                |
             CLI          Desktop GUI      Future facades
                                           MCP / Python
```

The CLI is a facade over application/session and domain capabilities. Automation semantics belong there, rather than in command parsing or Qt widgets. The future Automation API is a shared application boundary, not necessarily a separate service or a new framework. GUI and CLI use the same session concepts, validation, measurements and diagnostics. Neither owns a second timeline.

Expose SimNodus projects, components, pins, signals and measurements. Backend vector names, emulator calls and STM32 register interpretation stay in adapters/platform descriptions; they are not the primary public automation contract. A consumer should not need to know which backend supplies a quantity. Availability still depends on the validated device/model profile; neutral naming does not promise unsupported observations.

## Required product contract

Relevant operations must provide deterministically parseable structured results. JSON is the leading candidate, not a selected final wire format. Automation must not parse free-form human messages to determine success, failures or values.

- Use stable diagnostic/error codes and stable entity/instance IDs where applicable; retain human explanations alongside them.
- Preserve severity, provenance, units, explicit reference/orientation, effective virtual time and availability where meaningful. Validation diagnostics may have no simulation time.
- Provide a documented, stable exit-code policy for scripts and CI. Numeric assignments and error categories are open; do not infer a product policy from experimental validators.
- Keep human progress/log output distinguishable from structured results. Stream framing, stdout/stderr allocation, schema versioning and compatibility rules remain open.
- Identify the selected project, dependencies/backend versions, configuration, firmware identity/hash, temporal profile, tolerances and relevant seeds in reproducibility metadata.
- Require no GUI state, window focus, desktop session or interactive input for a prepared headless run. Missing inputs or unsupported operations must produce explicit failures, not unattended prompts.
- Make diagnostics and results available programmatically, including failures and unavailable or truncated data.

Parseability is distinct from numerical reproducibility: retain stable discrete traces in the same environment and published analog tolerances under [simulation time](SIMULATION_TIME.md). No cross-platform bit-identical analog result is promised.

## Conceptual operations and time

| Area | Possible operations, not command syntax |
|---|---|
| Project | Validate and inspect explicitly selected project data |
| Simulation | Run, pause, reset and inspect status within supported capabilities |
| Signal | List, inspect and capture observations |
| Measurement | Read a shared measurement definition/result |
| Component | List and inspect instances and pins |
| Firmware | Inspect explicit image and platform metadata |
| Diagnostics | List structured reports |
| Export | Export retained traces/results, potentially CSV |

A request to run until a virtual-time limit, for example 100 ms, goes through the same application/session and kernel as the GUI. Requested time, accepted effective time and committed time must not be conflated. Respect [causality](SIMULATION_TIME.md), the [temporal capability profile](TEMPORAL_CAPABILITY_PROFILE.md), [backend contracts](BACKEND_CONTRACTS.md) and [debug coordination](DEBUGGING.md). A pause request is not proof of a joint pause; reject unsupported operations. No new prediction, rollback, synchronous pause or general causal feedback capability follows from adding a facade.

## Shared measurements and project data

CLI, probes, schematic meters, Signal Analyzer and exports consume the same [measurement definitions](DESKTOP_UX.md#shared-definitions-and-data-ownership) and [instrumentation](DEBUGGING.md#instruments). Preserve stable IDs, explicit references/orientation, committed records and the distinction between configured, observed and derived values. The CLI must not create parallel acquisition or infer values absent from the selected backend profile.

The desktop's conceptual Analysis API is the shared analysis/distribution boundary; automation consumes it through application capabilities rather than duplicating it. Window placement, selected widgets and display downsampling do not define headless measurements. Retention and capture limits still apply.

Use the [project format](PROJECT_FORMAT.md) and its independent validation/resource/runtime gates. Agents may modify the external editable project representation, then validate and explicitly prepare a new run; external editing is not an implicit live-session mutation. Results and reproduction manifests remain separate from editable sources. This direction changes no declaration schema or persistence implementation.

## External agents and future facades

An intended agent workflow is: inspect project -> validate -> edit external project representation -> validate again -> run -> read measurements -> inspect diagnostics -> iterate. SimNodus supplies structured, deterministic operations; no embedded AI model is required.

MCP is one possible future facade over the same Automation API, not a core dependency. Conceptual tools might open/validate a project, run a simulation, list signals, inspect components, read measurements, capture waveforms and list diagnostics. No server or tool schema is selected.

Python clients, scripting and notebooks could reuse the same contracts for academic workflows and dataset generation. No bindings or Python runtime dependency are selected. JSON-RPC, stdin/stdout, named pipes, sockets and HTTP remain undecided transports; existing fixture pipes do not select a future public transport.

## Security and unattended execution

Apply the existing [security policy](../../SECURITY.md), [project path rules](PROJECT_FORMAT.md#paths-and-security) and [extension limits](COMPONENTS.md) equally to all facades. A CLI/API or agent connection grants no unrestricted host execution.

Project opening cannot run firmware on the host, invoke compilers, fetch dependencies or load native libraries implicitly. Firmware execution requires explicit simulation preparation and the compatible validated backend/profile. External models, WASM, native plugins and HDL retain their trust and capability gates. Preserve bounded filesystem access, input sizes, memory, queues, execution time and cancellation/failure handling. WASM filesystem/network access remains disabled by default; native process isolation alone is not a sandbox.

Prepared CI runs should validate examples and teaching firmware/circuits, execute regressions, compare results with declared tolerances, inspect diagnostics and retain traces/artifacts. GitHub Actions is a possible consumer alongside other CI systems. Existing [quality checks and engine workflows](../development/QUALITY.md) remain the evidence baseline; this document adds no pipeline or CI support claim. Untrusted CI inputs must not acquire privileged tokens or private resources.

## Scope and reconciliation

| Horizon | Scope |
|---|---|
| M2/M3 and teaching MVP | Preserve reproducible headless runs; design the limited exposed product operations around shared application capabilities, structured results/diagnostics, predictable exits, explicit inputs and virtual time. Exact operation coverage needs separate acceptance criteria; the full catalog above is not an MVP commitment. |
| Near-term after a usable bounded product flow | Specify versioned result/exit contracts, regression artifact comparison and supported automation operations incrementally, with GUI/headless agreement and failure evidence. No delivery date is added. |
| Longer-term possibilities | MCP facade, Python bindings/clients, notebooks, dataset workflows and broader remote automation; none is a classroom prerequisite. |

The architecture already connected CLI and desktop to Session / Application under ADR 0001. Its earlier description of `apps/cli` as headless experiments was narrower, not an opposing decision; this explicitly broadens the purpose. [Application entry points](../../legacy/apps/README.md) also retained a stale claim that neither application existed: it now distinguishes bounded implemented runners from the pending product applications.

The [SN-017 acceptance](../experiments/SN-017-acceptance.md) deliberately retains fixture-specific text I/O, endpoint ownership and Python/C# scheduling/GDB transport. Those measured experimental boundaries remain valid, not a product Automation API or a reason to move all automation into CLI parsing. Reference validators likewise keep their local exit codes and draft diagnostic formats. No existing protocol, evidence or historical ADR is replaced by this design, and SN-021 remains independent and unchanged.

## Open decisions and future evidence

Before publishing a stable interface, decide exact operations and naming, schemas and evolution, exit-code categories/numbers, framing/log separation, session lifetime, cancellation, capture/export limits and API ownership details. Define transport/authentication only if a separate process or remote facade is selected. Measurement persistence/migration and supported quantities remain governed by the existing instrumentation/project work.

Future ADRs are appropriate for the public compatibility/exit policy and, when needed, transport and trust boundaries. A separate measurement schema ADR may be needed when persistence is specified. No additional ADR is needed merely to list candidate command names. Acceptance must demonstrate real supported runs, GUI/headless measurement agreement where both exist, reproducible artifacts and bounded failure behavior; this documentation proves none of those capabilities.
