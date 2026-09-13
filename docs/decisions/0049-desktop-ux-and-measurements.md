# ADR 0049: Independent editor/analyzer windows and shared measurements

Date: 2026-09-13. Status: accepted owner direction; implementation pending.

## Context

The teaching interface needs clear editing and immediate circuit observation
alongside detailed signal analysis, on one or multiple monitors. SN-044 records
the owner's UI/UX request as documentation only, without advancing SN-020.
[ADR 0027](0027-shared-instrumentation.md) already defines shared instrumentation;
the UI must present that infrastructure without duplicating it.

## Decision

Adopt independent Circuit Editor and Signal Analyzer top-level windows, which
may share an executable/session. Keep editing, controls and quick observation
in the editor and detailed dataset analysis in the analyzer. Preserve future
multiple analysis windows. Specify component preview before insertion, distinct
instance properties, and resizable/collapsible side panels.

Use contextual temporary inspection and explicitly created persistent probes.
Persistent measurements use stable domain/instance identities and explicit
quantity/reference/orientation, not labels alone. Share definitions and data
across meters, tooltips, analyzer channels and exports. The analyzer consumes
retained instrumentation data and never becomes its primary store or time owner.

The [desktop UX design](../architecture/DESKTOP_UX.md) owns presentation details
and MVP/future scope. [DEBUGGING](../architecture/DEBUGGING.md#instruments)
continues to own acquisition, stores, decoding and timing. Widgets versus
Quick/QML, plotting, Console/Diagnostics placement and layout persistence
implementation remain open. Workspace persistence is desirable; Focus Circuit
and detach/reattach remain design considerations. Names/sketches are illustrative.

## Alternatives

- Confining analysis to a bottom panel restricts detailed and multi-monitor work.
- Per-instrument acquisition or analyzer-owned primary storage duplicates the
  existing instrumentation and ties capture lifetime to a view.
- Text-label-only probes lose their associations on ordinary renames.
- Selecting a Qt presentation technology now would preempt SN-022's evaluation.

## Consequences

Future GUI work must preserve session context across windows, restore usable
panel sizes, and keep shared measurements consistent across consumers. Closing
a view does not invalidate retained captures, but retention limits still apply.
Headless acquisition/analysis and the kernel's committed-time rules remain intact.

The minimal editor, essential scope/logic views and UART terminal remain M3
work; advanced analog analysis, stacked protocol decoding and multiple analyzer
windows are future direction. No toolkit, dependency, schema or simulator code
is selected or implemented here. No conflict with ADRs 0001/0027/0028 or current
persistence requirements was found; no ADR is superseded and no implementation
task status or milestone date changes.

## Revisit criteria

Use SN-022 desktop and worker-boundary evidence to select Qt modules in a future
ADR. Refine measurement semantics, capture retention and analysis interfaces
when SN-024 needs concrete contracts. Revisit window/layout details using
single-monitor and multi-monitor classroom usability evidence, without moving
timing or acquisition ownership into the GUI.
