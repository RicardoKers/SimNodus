"""Consume only an audited managed-revision fixed E-01 netlist in real ngspice."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tests/resources"))
import audit_managed_lifecycle as audit
sys.path.insert(0, str(ROOT / "tests/experiments/ngspice"))
from fixed_replay_acceptance import pinned_netlist
import run as e01


def digest(data):
    return sha256(data).hexdigest()


def compile_exact(project, package, binary):
    fields = (project, str(package).encode(), b"e01", b"reference", b"source", b"left_out")
    packet = b"".join(struct.pack("<I", len(item)) + item for item in fields)
    process = subprocess.run([str(binary), "replay"], input=packet, capture_output=True,
                             timeout=30, check=True)
    lines = process.stdout.decode("ascii").splitlines()
    assert lines[0] == "OK" and bytes.fromhex(lines[2]) == project and lines[3] == "0 0"
    elements = [line.split() for line in lines[4:6]]
    assert [entry[0] for entry in elements] == ["main/left/r", "main/left/c"]
    source = (package / "tests/schema/fixtures/assets/passive.cir").read_bytes()
    for entry in elements:
        assert json.loads(project[int(entry[3]):int(entry[4])])["id"] == entry[0].split("/")[-1]
        assert source[int(entry[7]):int(entry[8])].startswith(b".subckt ")
    replay = next(line.split() for line in lines[7:] if line.startswith("REPLAY "))
    assert list(map(int, replay[1:3])) == [5_000_000, 5_000_000]
    document = json.loads(project)
    assert json.JSONDecoder().raw_decode(project[int(replay[4]):].decode())[0] == document["temporal"]
    assert json.JSONDecoder().raw_decode(project[int(replay[5]):].decode())[0] == document["temporal"]["schedule"]
    return bytes.fromhex(lines[1]), elements, replay


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vm-run", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    run = args.vm_run.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {"status": "started", "vm_run": run.name,
              "voltage_tolerance_v": e01.VOLTAGE_TOLERANCE,
              "time_tolerance_s": e01.TIME_TOLERANCE}
    try:
        checked = audit.inspect(run)
        report["byte_audit"] = checked
        with (output / "assets.log").open("w", encoding="utf-8") as log:
            subprocess.run([sys.executable, str(ROOT / "tools/check_backend_assets.py"), "--ngspice-only"],
                           cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        package = output / "package"
        package.mkdir()
        for name, relative in (("LICENSE", "LICENSE"), ("passive.svg", "tests/schema/fixtures/assets/passive.svg"),
                               ("passive.cir", "tests/schema/fixtures/assets/passive.cir"),
                               ("fixed-drive.csv", "fixed-drive.csv")):
            destination = package / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(run / name, destination)
        project = (run / "artifacts/edited.json").read_bytes()
        (output / "edited.json").write_bytes(project)
        compiler = ROOT / "build/sn021-managed-vm/Release/native_rc_compile_probe.exe"
        compiled, elements, replay = compile_exact(project, package, compiler)
        vm_netlist = (run / "artifacts/managed-rc.cir").read_bytes()
        assert compiled == vm_netlist and digest(vm_netlist) == checked["netlist_sha256"]
        report["compiler_sha256"] = digest(compiler.read_bytes())
        report["source_map"] = elements
        report["replay"] = replay
        stage = output / "stage"
        stage.mkdir()
        (stage / "rc.cir").write_bytes(vm_netlist)
        deps = ROOT / "build/deps"
        command = [str(ROOT / "build/e01/Debug/e01.exe"),
                   str(deps / "ngspice/Spice64_dll/dll-vs/ngspice.dll"),
                   str(deps / "ngspice-console/Spice64/bin"),
                   str(ROOT / "tools/backend_probe/initialization"), "pending-stage", "rc", str(output)]
        with pinned_netlist(stage, vm_netlist) as pinned:
            command[4] = str(pinned)
            report["command"] = [value.replace(str(ROOT), "<repository>").replace(str(pinned), "<pinned-stage>")
                                 for value in command]
            with (output / "engine.log").open("w", encoding="utf-8") as log:
                process = subprocess.run(command, cwd=output, stdout=log,
                                         stderr=subprocess.STDOUT, timeout=30)
            assert (stage / "rc.cir").read_bytes() == vm_netlist
        report["process_exit"] = process.returncode
        assert process.returncode == 0
        metrics = json.loads((output / "metrics.json").read_text(encoding="utf-8"))
        report["metrics"] = metrics
        assert metrics["idle_before_quit"] == metrics["quit_requested"] == 1
        assert metrics["exit_status"] == metrics["callback_fault"] == 0
        report["analytical"] = e01.analyze(output / "first.csv", False, 0.005)
        policy = json.loads(project)["temporal"]
        report["project_voltage_tolerance_v"] = policy["voltage_tolerance_uv"] * 1e-6
        report["project_time_tolerance_s"] = policy["analog_time_tolerance_ps"] * 1e-12
        assert report["analytical"]["max_error_v"] <= report["project_voltage_tolerance_v"]
        assert abs(report["analytical"]["final_time_s"] - policy["duration_ns"] * 1e-9) <= report["project_time_tolerance_s"]
        callbacks, vectors = e01.rows(output / "callbacks.csv"), e01.rows(output / "first.csv")
        assert len(callbacks) == len(vectors)
        assert all(all(left[key] == right[key] for key in ("time_s", "input_v", "output_v"))
                   for left, right in zip(callbacks, vectors))
        report["callbacks_match_vectors"] = True
        report["netlist_sha256"] = digest(vm_netlist)
        report["status"] = "passed-managed-rc-lifecycle-e01-only"
    except Exception as error:
        report["status"] = "failed"
        report["error_type"] = type(error).__name__
        report["error"] = str(error)
        raise
    finally:
        report["files_sha256"] = {path.name: digest(path.read_bytes()) for path in output.iterdir() if path.is_file()}
        (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({key: report.get(key) for key in ("status", "error", "analytical", "netlist_sha256")}))


if __name__ == "__main__":
    main()
