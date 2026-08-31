# Sources and requirement provenance

Research date: 2026-08-31. External pages and upstream branches can change; pin revisions during SN-010. Source inspection is not execution validation.

## Owner-provided context

Two linked architecture conversations were accessed through the conversation reader. The relevant owner messages were available; long assistant architecture messages were truncated by the connector at its per-message limit. No attachments were listed. The foundation uses the available content and does not claim to have recovered omitted text.

The conversations established the teaching goal, STM32F103/Blue Pill focus, ngspice + Renode direction, C++/Qt, separate visual/electrical component models, simple subcircuits, WASM/HDL interest, diagnostics, and CubeIDE debugging.

The current task confirmed the name SimNodus and requested local architecture/planning plus future GitHub preparation. The owner's follow-up confirmed English repository files, Windows first/Linux later, public open source, authority to choose a permissive license, and February 2027 classes.

These are synthesized requirements, not instructions copied from assistant suggestions. Raw transcripts and private conversation identifiers are intentionally excluded from public-ready files.

## Primary technical references

| Reference | What it supports | What it does not prove |
|---|---|---|
| [Renode external-control documentation](https://renode.readthedocs.io/en/latest/api-description/external-control.html) | External control, time/I/O facilities | Causal feedback behavior in SimNodus |
| [Renode client header](https://raw.githubusercontent.com/renode/renode/master/tools/external_control_client/include/renode_api.h) | Concrete API declarations for the retrieved revision | Compatibility with an arbitrary release |
| [Renode time framework](https://renode.readthedocs.io/en/latest/advanced/time_framework.html) | Virtual time, quanta, pause/halt distinctions | Cycle accuracy or automatic ngspice synchronization |
| [Renode GDB](https://renode.readthedocs.io/en/latest/debugging/gdb.html) | GDB remote debugging facilities | A tested CubeIDE launch configuration or coupled pause |
| [STM32F103 platform description](https://github.com/renode/renode/blob/master/platforms/cpus/stm32f103.repl) | Declared peripheral/platform wiring | Complete, accurate F103C8 behavior |
| [STM32F103C8 product page](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8) and [documentation](https://www.st.com/en/microcontrollers-microprocessors/stm32f103/documentation.html) | Hardware memory/peripheral baseline and register manual location | Fidelity of Renode's implementation |
| [ngspice shared library](https://ngspice.sourceforge.io/shared.html) | Host control and callbacks | A finished Renode coupling algorithm |
| [ngspice documentation](https://ngspice.sourceforge.io/docs.html) | Manual and API investigation starting point | Compatibility of every vendor model |

## Findings requiring careful interpretation

The recent Renode API documentation includes nanosecond time units; a separately retrieved older header exposed microsecond units and GPIO timestamps. This is a version/cache discrepancy, not proof that the current API is microsecond-only. Match executable, client, headers, and documentation.

The inspected generic STM32F103 platform description declares GPIO, timers, interrupts, and other peripherals but does not visibly declare an ADC instance in that file. A board overlay or another revision can change this; inspect the selected complete configuration. Never infer all peripheral coverage from a platform name.

Renode's documented GDB facilities justify an experiment, not an already-supported SimNodus feature. The original discussions were optimistic about whole-circuit debugging; E-05 is the required validation gate.

E-02 records fourteen exact Renode-infrastructure source URLs and hashes in `tests/experiments/renode-stm32/audit-sources.json`. The files are downloaded only for read-only audit and are not committed or compiled. Its CI compiler archive and vendor checksum source are recorded separately in `ci-toolchain.json`; the compiler is not redistributed.

## Licensing and automation references

- [MIT text](https://opensource.org/license/mit) and [MIT summary](https://choosealicense.com/licenses/mit/).
- [Renode license](https://github.com/renode/renode/blob/master/LICENSE).
- [ngspice developer licensing overview](https://ngspice.sourceforge.io/devel.html).
- [Qt licensing by module](https://doc.qt.io/qt-6/licensing.html).
- [Checkout v7.0.1 commit](https://github.com/actions/checkout/commit/3d3c42e5aac5ba805825da76410c181273ba90b1), used by the structural CI workflow.

No fresh trademark/domain clearance was performed. Earlier name-search claims are not treated as clearance.
