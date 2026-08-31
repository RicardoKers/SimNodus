# Test strategy

The [E-01 standalone ngspice suite](experiments/ngspice/README.md) runs eight real Windows backend cases and checks analytical RC references, callbacks, pauses, resets, and invalid-netlist recovery. See its [measured scope](../docs/experiments/E-01-results.md). Production kernel, coupled firmware/circuit, and GUI tests do not exist yet. The repository checker validates documentation/scaffolding only.

Create domain/kernel invariant tests as implementation appears, real-backend contract tests during experiments, and integrated firmware/circuit/debugging tests before the GUI is declared usable. Follow [QUALITY](../docs/development/QUALITY.md); do not count mocks as real integration evidence.

[SN-019](experiments/renode-client/README.md) separately checks native Windows control against real Renode and uses scripted local peers for fault injection. Its empty-machine timing evidence does not validate firmware, GPIO/ADC, or coupled execution.
