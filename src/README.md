# Simulation source layout

No engine sources are implemented yet. Add modules as the experiments establish real contracts, rather than filling directories with placeholder classes.

| Planned directory | Responsibility |
|---|---|
| `domain/` | Circuit graph, component/pin/net identity, hierarchy, project data |
| `core/` | Virtual time, events, session states, coordination |
| `coupling/` | Electrical/digital/ADC boundary behavior |
| `adapters/ngspice/` | ngspice API and lifecycle |
| `adapters/renode/` | Renode process/client/platform integration |
| `application/` | Session commands, loading/saving, orchestration |
| `instrumentation/` | Trace storage/export and diagnostics |

Future targets use the `simnodus::` namespace and explicit dependencies. Domain/core must not depend on Qt. See the [architecture](../docs/architecture/README.md).
