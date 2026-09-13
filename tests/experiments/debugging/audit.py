"""Verify the exact Renode sources on which E-05's transport bootstrap depends."""
from __future__ import annotations

import argparse
import hashlib
import json
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT / "build/sn016/audit")
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((HERE / "audit-sources.json").read_text(encoding="utf-8"))
    fingerprints = {}
    for entry in manifest["files"]:
        path = args.root / entry["file"]
        if args.download:
            with urllib.request.urlopen(entry["url"], timeout=30) as response:
                data = response.read(1024 * 1024)
            path.write_bytes(data)
        else:
            data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != entry["sha256"]:
            raise ValueError(f"Source fingerprint mismatch: {path.name}")
        fingerprints[path.name] = digest

    provider = (args.root / "SocketServerProvider.cs").read_text(encoding="utf-8")
    required = [
        "private volatile bool stopRequested;",
        "private Thread listenerThread;",
        "private Socket server;",
        "private void ListenerThreadBody()",
        "new IPEndPoint(IPAddress.Any, port)",
    ]
    if not all(fragment in provider for fragment in required):
        raise ValueError("Audited SocketServerProvider private shape changed")
    stub = (args.root / "GdbStub.cs").read_text(encoding="utf-8")
    if "public GdbStub(IMachine machine, IEnumerable<ICpuSupportingGdb> cpus, SocketServerProvider terminal)" not in stub:
        raise ValueError("Audited GdbStub injected-transport constructor changed")
    machine = (args.root / "Machine.cs").read_text(encoding="utf-8")
    if "public void StartGdbServer(SocketServerProvider terminal, IEnumerable<string> cpuNames = null)" not in machine:
        raise ValueError("Audited Machine injected-transport overload changed")

    report = {
        "revision": manifest["revision"],
        "source_sha256": fingerprints,
        "standard_listener": "IPAddress.Any",
        "experiment_listener": "IPAddress.Loopback",
        "private_shape_verified": True,
    }
    (args.root / "audit.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(fingerprints)} E-05 GDB sources. No server was started.")


if __name__ == "__main__":
    main()
