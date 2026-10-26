"""Shared input thresholds for the behavioral logic parts (robustness pass, 2026-10-25).

A part's equations test the same inputs many times with the same smooth
threshold, 0.5*(1+tanh(...)); a decade counter repeats 7 of them 56 times.
ngspice evaluates each copy and its derivative at every Newton iteration, which
made example 23 run at half of real time. share_levels() computes each distinct
threshold once, as the voltage of a behavioral source (node lv<n>_{ref}), and
lets the equations read that voltage. The equations are the same, so are the
results; the run is about twice as fast.

Only for parts whose state is a capacitor that keeps its voltage while no
current flows (latches, flip-flops, counters, the decoders' latches). Not for
the 555: its latch holds itself through its own threshold, a bistable that the
first step of an interactive segment (uic) can flip when the thresholds are
separate nodes; its pulse then ended at the segment boundary.
"""

HEAD = "0.5*(1+tanh("


def closing(text, start):
    """Index after the parenthesis that closes the one at `start`."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return i + 1
    raise ValueError(f"unbalanced parentheses: {text}")


def share_levels(lines):
    """`lines` with every repeated threshold expression read from a shared node."""
    count = {}
    for line in lines:
        start = line.find(HEAD)
        while start >= 0:
            end = closing(line, start + len("0.5*"))
            count[line[start:end]] = count.get(line[start:end], 0) + 1
            start = line.find(HEAD, end)
    shared = {expression: f"lv{n}_{{ref}}" for n, expression in
              enumerate(e for e, c in count.items() if c > 1)}
    if not shared:
        return list(lines)
    out = []
    for line in lines:
        start = line.find(HEAD)
        while start >= 0:
            end = closing(line, start + len("0.5*"))
            node = shared.get(line[start:end])
            if node:
                line = line[:start] + f"V({node})" + line[end:]
                end = start + len(node) + 3
            start = line.find(HEAD, end)
        out.append(line)
    return [f"B_{{ref}}_{node[:-6]} {node} 0 V={expression}" for expression, node in shared.items()] + out
