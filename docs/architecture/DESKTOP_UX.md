# Desktop UX and visual instrumentation

Status: owner-confirmed window and measurement direction under
[ADR 0049](../decisions/0049-desktop-ux-and-measurements.md), recorded by SN-044.
No GUI or instrumentation implementation is delivered here. Layout sketches,
instrument names and measurement expressions are conceptual, not required class,
widget or API names. [Debugging and instruments](DEBUGGING.md#instruments) remains
the authority for acquisition, stores, decoding, diagnostics and timing.

## Two first-level windows

The application has two independent top-level windows: **SimNodus Circuit Editor**
and **SimNodus Signal Analyzer**. They may share an executable and simulation
session; separate windows do not imply separate processes or time owners.

| Window | Responsibility |
|---|---|
| Circuit Editor | Assemble/edit circuits, select/place components, wire nets, edit instance properties, interact with virtual controls, inspect instantaneous values and send observations to analysis |
| Signal Analyzer | Inspect signal history and datasets through temporal, spectral, digital and protocol views, detailed measurements, cursors and exports |

Detailed analysis must not be confined to a small bottom panel in the editor.
Both windows must remain usable on one monitor, with the editor and analyzer
also usable on separate monitors. Preserve the possibility of multiple analysis
windows later, such as analog analysis on a second monitor and logic/protocol
analysis on a third. Multiple analyzer windows are future scope.

## Circuit Editor layout

```text
+-------------------------------------------------------------------+
| File Edit View Simulation Instruments Debug              Run/Stop |
+--------------+-----------------------------------+----------------+
| COMPONENTS   |                                   | PROPERTIES     |
| categories   |                                   | selected       |
| search/list  |          CIRCUIT CANVAS           | instance       |
|              |                                   | properties     |
|--------------|                                   |                |
| PREVIEW      |                                   |                |
| symbol/info  |                                   |                |
+--------------+-----------------------------------+----------------+
| optional status / diagnostics / simulation state                  |
+-------------------------------------------------------------------+
```

This is a conceptual arrangement, not a fixed menu or widget specification.
Keep the canvas central, avoid visual clutter, support teaching and discoverable
actions, and enable efficient keyboard and mouse operation. Preserve circuit and
measurement context while moving complex analysis out of the canvas. GUI refresh
and user interaction must remain decoupled from simulation timing.

The component browser provides categories, search, clear identification, rapid
navigation and keyboard/mouse selection. A Component Preview beneath it follows
selection and search before insertion. Its primary purpose is to confirm the
selected library symbol visually. Where useful, show name, pin count, category,
short description and model kind (analog, digital, MCU or subcircuit). Declared
model kind is not proof of validated simulation support.

Preview describes an available library item; Properties/Inspector describes an
instance already in the circuit. Preserve the [component separation](COMPONENTS.md)
between symbol, connectivity and model. Preview must not require insertion or
model execution.

Components with Preview and Properties/Inspector must be resizable, collapsible
and reopenable at their previous size. Editing exposes components, canvas and
properties; viewing allows the canvas to occupy almost the whole window.
Consider a temporary "Focus Circuit" action that hides panels and later restores
the preceding layout. Its name, shortcut and exact interaction remain open.

Prefer persistence between sessions of main-window size/position, panel widths,
visibility/collapsed state, analyzer geometry, last monitor and workspace
organization where feasible. Storage format and implementation are undecided;
restoration should keep windows reachable when a monitor is unavailable.
Console and Diagnostics may belong to the editor or detachable panels/windows;
their placement remains open and need not be inside the analyzer.

## Observation and contextual probes

Simple schematic instruments provide immediate observation: voltmeter, ammeter,
wattmeter, frequency meter and digital indicators are examples. A visual meter
may display `3.287 V` or `12.4 mA` beside the measured entity. Every such consumer
uses the shared instrumentation described below, with units, reference/orientation,
effective observation time and availability. "Instantaneous" means the available
committed observation, not a promise of continuous real-time measurement.

Contextual probes expose quantities supported by the selected entity/model:

| Context | Conceptual quantities |
|---|---|
| Node / wire / net | Voltage relative to an explicit reference, e.g. `Voltage(net, reference)` |
| Terminal / pin | Current with explicit entering/leaving orientation; other model-supported quantities |
| Component | Own or derived quantities: resistor voltage/current/power; MOSFET VDS, VGS, ID, IG and power dissipation |
| Digital / MCU pin | Logic state, transitions, frequency, duty cycle and send to Logic Analyzer |

These examples do not assert backend or peripheral coverage. Unavailable values
must be identified, and configured, observed and derived quantities distinguished
as in [circuit/firmware correlation](DEBUGGING.md#circuit-and-firmware-correlation).

Temporary inspection uses hover or a quick click without permanently modifying
the project. A conceptual PA8/PWM_GATE tooltip could show voltage, logic,
frequency and duty cycle if those observations exist. A persistent probe is
explicitly created by the user, remains associated with its measured entity,
and can appear on the schematic, be saved, feed analysis and participate in
exports. Saving a definition does not imply saving its captured history.

Provide a simple route such as context menu -> Add Probe -> Open in Signal
Analyzer, or drag a probe onto a net/component/pin -> choose a quantity -> display
the trace. Exact gestures remain design details. Common measurements must use
circuit entities and understandable quantities without requiring ngspice syntax
such as `V(n001)` or `I(R7)`.

## Shared definitions and data ownership

Persistent measurements refer to stable domain IDs, including instance identity,
quantity, voltage reference and current orientation as applicable. Labels are
presentation metadata: renaming a net/component must not automatically break a
probe. In `Voltage(net_17, GND)`, GND is a display label for an explicitly resolved
reference, not an implicit connection to every entity named GND. Deleted or
unresolvable targets must be reported rather than silently rebound by name;
concrete migration and persistence rules remain open.

```text
Shared measurement definition: Voltage(net_17, reference_id)
    +-- schematic voltmeter
    +-- temporary tooltip
    +-- oscilloscope channel
    +-- CSV exporter
```

This extends [shared records](DEBUGGING.md#signal-and-event-records): definitions,
acquisition and derived measurement semantics are shared across consumers, not
reimplemented independently by each instrument. Exact types are not selected.

```text
Simulation Core / kernel-owned Simulation Timeline
                      |
             shared Instrumentation
                      |
          +-----------+------------+
          |                        |
     Signal Store          Digital Event Store
    (analog samples)        (digital transitions)
          |                        +--> DecoderManager --> annotations
          +------------------------+-----------+
                                   |
                    shared analysis/distribution boundary
                                   |
                  +----------------+----------------+
                  |                |                |
            Editor meters    Signal Analyzer   exports / future
             and probes          views        headless analysis
```

Digital Event Store is the Event Store already described in DEBUGGING.md; an
"Analog Store" means its Signal Store. The analysis boundary is a conceptual
Analysis API over that same infrastructure, not a competing subsystem or fixed
interface. DecoderManager and native/optional Python/sigrok paths remain as
documented in ADR 0027. The analyzer presents their results, not protocol logic.

The analyzer is not primary measurement storage. Closing/reopening it must not
by itself destroy retained session data; multiple views, exports and future
headless analysis consume the same observations. Retention limits still apply:
reopening cannot recover uncaptured or discarded history. Disk persistence and
capture lifetime beyond the session remain undecided. Presentation owns view
state, while instrumentation owns acquisition/storage/distribution. No GUI
window calls backends directly or owns simulation causality. Existing committed
record, approximate-mode, bounded-memory and event-preservation rules apply.

## Initial scope and future analysis

The [M3 boundary](../REQUIREMENTS.md#mvp-boundary) and existing SN-023/SN-024
remain the teaching baseline; the full instrument catalog is not an MVP promise.

| Stage | UX scope |
|---|---|
| M3 teaching baseline | Minimal editor with component preview, wiring and instance properties; adjustable/collapsible side panels; basic contextual voltage/current/digital observation; simple send/open flow to a separate analyzer with essential scope and logic views, exportable traces, plus the already planned UART terminal and debug workflow |
| Desired, subject to usability/scope review | Focus Circuit action, persisted workspace/monitor geometry and detach/reattach behavior; exact initial schematic meter set and detailed interaction design |
| Future analog analysis | Multiple voltage/current channels, zoom/pan, cursors, delta-time, frequency, RMS, min/max, average, mathematical traces, FFT/spectrum, harmonic analysis, AC sweep/Bode magnitude and phase, XY plots |
| Future digital analysis | Multiple digital lanes, temporal zoom/pan, groups/buses, cursors, frequency/duty measurements, protocol annotations and stacked decoders; UART/SPI/I2C, later CAN and other validated protocols |
| Future workspace expansion | Multiple analyzer windows, richer circuit/firmware correlation and headless analysis tools |

Basic views may use navigation or multiple channels earlier as needed; the
comprehensive analysis suite above is future direction, not additional M3
acceptance. UART terminal/logging does not imply a completed protocol decoder.
Frequency/duty displays and other derived measurements depend on captured data
and agreed semantics. AC/FFT features require suitable datasets and analysis
capabilities; a GUI decision does not establish backend support.

PulseView is a functional/conceptual reference for digital analysis only, not
an appearance to copy or a selected dependency. For example SCK/MOSI/MISO lanes
may feed an SPI annotation row of bytes, then a higher-level CMD/VALUE/CRC row.
Stacked decoding is future work, subject to the existing decoder boundaries.

## Open choices and decision gates

Qt Widgets versus Qt Quick/QML remains open. SN-022 should seriously evaluate
Widgets against these desktop needs: multiple top-level windows, dockable panels,
splitters, collapsing, geometry persistence and detach/reattach. These are
evaluation criteria, not a claim of measured toolkit suitability or a selection.
Preserve Qt 6 in presentation only; no prototype is required by this document.

SN-022 should record the eventual Qt module/worker choice in an ADR with usability,
licensing and failure-boundary evidence. During SN-024, consider a separate ADR
when measurement identity, derived quantities, retention and the analysis API
become concrete; preserve ADR 0027 rather than creating another data pipeline.
Console/Diagnostics placement, focus shortcuts, workspace persistence location,
plotting library and multiple-view lifecycle details remain open.

No incompatible existing decision was found: ADR 0001 leaves Qt presentation
details open, ADR 0027 already requires shared GUI-independent instrumentation,
and persistence distinguishes editable sources from results. Existing M3 scope
and future decoder direction are retained. SN-020 schema work and all simulator
capability claims remain unchanged.
