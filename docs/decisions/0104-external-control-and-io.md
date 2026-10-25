# ADR 0104: External control through one command vocabulary; external I/O as a discreet built-in link

Date: 2026-10-25. Status: accepted (owner's direction 2026-10-25; SN-215 and
SN-216 implemented, SN-217 and SN-218 planned). Makes ADR 0055 concrete.

## Context

The owner made external connectivity the next priority, for two uses:

1. **AI agents** (Claude Code, Codex) controlling the simulator: automated
   simulations, and video tutorials in which an agent builds a circuit step by
   step on screen.
2. **The owner's virtual industrial plants** (Godot): conveyors, motors,
   actuators and sensors that an external simulator controls. The plant is a
   passive server (WebSocket `ws://127.0.0.1:8811`, or TCP `127.0.0.1:8765`
   with one JSON object per line); the client sends commands (`%QX…` booleans,
   `%QW…` integers) and gets every input back (`%IX…`, `%IW…`). It must stay
   discreet in SimNodus, whose purpose is circuits and microcontrollers.

ADR 0055 required automation over the same session and project capabilities as
the GUI, with structured results and stable codes, but chose no interface.

## Decision

- **One vocabulary of JSON commands** in a Qt-free library (`src/automation`):
  `{"command", "args", "id"}` in, `{"ok", "result" | "error": {code, message}}`
  out, with stable error codes and a protocol version. Commands act on a
  `Workspace` (project, library, session); edits go through `Workspace::apply`,
  so the desktop application can later route them through its undo stack.
  Components are named by designator, pins as `R1.2`, points on the grid.
- **Facades, in this order:**
  1. `simnodus-cli` (SN-215, done): headless, one-shot commands (`run`,
     `check`, `inspect`, ...) and `serve`/`exec` for JSON lines. Agents call it
     from their shell; it runs the session in process.
  2. A control channel in the desktop application (SN-216): a local endpoint,
     off by default, enabled explicitly; edits appear on the canvas and in the
     undo history, paced for recording.
  3. An MCP server (SN-217): a thin translation of the same commands into MCP
     tools.
- **Interactive runs stop at an asked simulated time**: `Session::pause_at`
  pauses at the first accepted point at or after it (engine protocol 8), so
  agents and tests get exact times instead of wall-clock polling.
- **External I/O for the plants** (SN-218, after the agent work): generic
  parts, not named after the plant: bit input and output, word input and
  output, each with a free-text address kept literally (`%IX0.10`), an explicit
  polarity, and a word scale of 0–1000 ↔ 0–3.3 V by default, changeable in the
  part's parameters. A link built into SimNodus (TCP, one JSON object per
  line; no new dependency) exchanges them with a server at a configurable host,
  port and period (20 ms by default). It is active only in projects that
  contain such parts and is configured in a single dialog. Stopping or pausing
  sends every mapped output's safe value (false, 0) explicitly, because the
  plant keeps the last command. The coupling is in wall-clock time and is
  labeled as not deterministic.

### The window's control channel (SN-216)

- **Transport:** a `QLocalServer` named `simnodus-control-<user>`, a named
  pipe on Windows and a socket in the temporary folder on Linux, created with
  user-only access. One JSON request per line, one reply line each, the same
  as `simnodus-cli serve`. No access token: only the user's own programs can
  open the endpoint, and a browser cannot reach a pipe or a local socket (a
  localhost TCP port could be reached by a web page sending a POST).
  `simnodus-cli connect` bridges standard input or a file to it, so agents
  use the same program as without a window.
- **Off by default and not remembered:** Tools > Allow External Control or
  `--control` for one session; the status bar shows when it is on and when a
  program is connected. Only one window can own the endpoint.
- **The workspace is the window:** edits go through the document's undo stack
  and are selected and scrolled into view; files open and save as the menus
  do, but unsaved changes are kept unless the request says `discard`; runs go
  through the run controller (canvas, Signal Analyzer) at the project's
  speed; a structural edit stops a run as manual editing does. Waiting
  commands run a nested event loop, so the window keeps drawing; requests
  arriving meanwhile are queued, and closing the window cancels the wait.
- **Commands of the window only** (`view.pace`, `view.caption`, `view.show`,
  `view.fit`, `view.wait`, `view.analyzer`) are added by the host through
  `Automation::extend`, so the vocabulary stays one list in `help`.

## Alternatives

- **Driving the GUI through screen automation** ("computer use"): no build
  work, but fragile and slow for repeatable tutorials.
- **MCP first**: ties the contract to one agent protocol; CLIs work with every
  agent today, and MCP can wrap the same commands.
- **An external script bridging SimNodus and the plant**: nothing
  plant-specific in SimNodus, but two hops and one more program for students
  to run. It stays possible once the control channel exists.
- **WebSocket for the plant**: the plant supports it, but it would add a
  library or our own framing; its TCP line protocol carries the same signals.

## Consequences

- Agents can already build, check, simulate and measure circuits, and save
  projects that open in the desktop application. The command list is in
  `docs/user/AUTOMATION.md` and in the `help` command.
- The command vocabulary becomes a contract: changes bump `protocol_version`.
- `simnodus-cli` is packaged next to the application and checked by the
  packaging script.
- The plant link adds network access on request only; projects never connect
  merely by being opened (the link starts with a run, after the user enabled
  it).

## Revisit criteria

Revisit the control channel's lack of a token if programs of other users or
remote machines ever need it (they cannot reach it now). Revisit the wall-clock
coupling of the plant link if lessons need deterministic co-simulation; that
would need a stepping protocol on the plant's side. Related: ADR 0055,
ADR 0080 (interactive runs), ADR 0101 (engine process).
