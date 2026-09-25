"""Copy selected existing VM records for an inert, independent local byte audit.

Manual host helper. Prompts for secrets without persisting them; performs only VIX
file copies, never guest program execution, cleanup, retry or snapshot changes.
"""

import ctypes as c
import getpass
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import uuid

from managed_record_correspondence import LEAF, REVISIONS, RUN, inspect


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
PUBLISHED = ROOT / "docs/experiments/evidence/SN-021-managed-store-attempt9-probe.json"
ORIGINAL = ROOT / "docs/experiments/evidence/SN-021-managed-store-attempt9-result.json"
RUNNER = BUILD / "sn021-vm-managed-store-run.py"
ACCESS = BUILD / "sn021-vm-access-check.py"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    if not sys.stdin.isatty():
        raise SystemExit("Use an interactive host terminal; do not redirect credentials.")
    baseline = json.loads(ORIGINAL.read_text(encoding="utf-8"))
    assert sha(RUNNER) == baseline["sources"][RUNNER.name]
    assert sha(ACCESS) == baseline["sources"][ACCESS.name]
    spec = importlib.util.spec_from_file_location("sn021_measured_runner", RUNNER)
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    destination = BUILD / "sn021-record-correspondence-runs" / uuid.uuid4().hex
    destination.mkdir(parents=True, exist_ok=False)
    report = {"status": "started", "source_run": RUN, "root_leaf": LEAF,
              "runner_sha256": sha(RUNNER), "files": {}}
    api = runner.Vix()
    host = vm = properties = 0
    logged_in = False
    try:
        host = api.job(api.d.VixHost_Connect(-1, 3, None, 0, None, None, 0, 0, None, None), True)
        secret = getpass.getpass("VMware encryption password (hidden): ").encode("utf-8")
        prop = c.c_int()
        error = api.d.VixPropertyList_AllocPropertyList(host, c.byref(prop), 7001, c.c_char_p(secret), c.c_int(0))
        del secret
        if error:
            raise RuntimeError("VIX property error " + str(error))
        properties = prop.value
        vm = api.job(api.d.VixHost_OpenVM(host, runner.access.VMX.encode(), 0, properties, None, None), True)
        api.d.Vix_ReleaseHandle(properties)
        properties = 0
        api.job(api.d.VixVM_WaitForToolsInGuest(vm, 30, None, None))
        secret = getpass.getpass("Windows guest password for SimNodus (hidden): ").encode("utf-8")
        api.job(api.d.VixVM_LoginInGuest(vm, b"SimNodus", secret, 0, None, None))
        del secret
        logged_in = True
        sources = [(f"C:\\{LEAF}\\generation.manifest", "generation.manifest"),
                   (f"C:\\SN021ManagedHarness-{RUN.rsplit('-', 1)[1]}\\project.json", "project.json")]
        sources += [(f"C:\\{LEAF}\\document\\{n:08x}.commit", f"{n:08x}.commit") for n in REVISIONS]
        for source, name in sources:
            target = destination / name
            try:
                api.job(api.d.VixVM_CopyFileFromGuestToHost(vm, source.encode("utf-8"),
                    str(target).encode("utf-8"), 0, 0, None, None))
                report["files"][name] = {"sha256": sha(target), "bytes": target.stat().st_size}
            except RuntimeError as failure:
                report["files"][name] = {"error": str(failure)}
                break
        report["status"] = "collected" if len(report["files"]) == len(sources) and all(
            "sha256" in item for item in report["files"].values()) else "partial"
    except (Exception, KeyboardInterrupt) as failure:
        report["status"] = "failed-or-indeterminate"
        report["error_type"] = type(failure).__name__
        if isinstance(failure, RuntimeError):
            report["error"] = str(failure)
    finally:
        if logged_in:
            try:
                api.job(api.d.VixVM_LogoutFromGuest(vm, None, None))
            except Exception:
                report["logout_failed"] = True
        if properties:
            api.d.Vix_ReleaseHandle(properties)
        if vm:
            api.d.Vix_ReleaseHandle(vm)
        if host:
            api.d.VixHost_Disconnect(host)
        (destination / "collection.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if report["status"] == "collected":
        try:
            outcome = inspect(destination, PUBLISHED)
            (destination / "analysis.json").write_text(json.dumps(outcome, indent=2) + "\n", encoding="utf-8")
            report["status"] = outcome["status"]
        except (ValueError, KeyError, IndexError, struct.error) as failure:
            report["status"] = "collected-but-unverified"
            report["analysis_error"] = str(failure)
        (destination / "collection.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Result: " + report["status"])
    print("Retained evidence: " + str(destination))
    print("Existing files only; no guest program, retry, cleanup or snapshot change.")


if __name__ == "__main__":
    main()
