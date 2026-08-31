# Test strategy

The [E-01 standalone ngspice suite](experiments/ngspice/README.md) runs eight real Windows backend cases and checks analytical RC references, callbacks, pauses, resets, and invalid-netlist recovery. See its [measured scope](../docs/experiments/E-01-results.md). Production kernel, coupled firmware/circuit, and GUI tests do not exist yet. The repository checker validates documentation/scaffolding only.

Create domain/kernel invariant tests as implementation appears, real-backend contract tests during experiments, and integrated firmware/circuit/debugging tests before the GUI is declared usable. Follow [QUALITY](../docs/development/QUALITY.md); do not count mocks as real integration evidence.
