"""Build and run the experimental CubeIDE startup probe in a fresh workspace.

The probe is partial evidence, never a complete E-05 compatibility pass.
The IDE installation and previous workspaces are read-only inputs.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import re
import shutil
import subprocess
import time
import zipfile
from pathlib import Path
from datetime import datetime, timezone
from types import SimpleNamespace
from gdb_guard import Guard
from cooperative_debug import CooperativeGuard, Coordinator
from joint_ide import JointCoordinator

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location("e05", HERE / "run.py")
e05 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e05)


def write_atomic(path: Path, text: str) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ide", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", choices=("normal", "reset", "pause", "lifecycle", "cooperative", "joint"), default="normal")
    parser.add_argument("--experimental-renode", type=Path)
    parser.add_argument("--joint-lifecycle", action="store_true")
    parser.add_argument("--joint-steps", action="store_true")
    parser.add_argument("--joint-breakpoint-first", action="store_true")
    parser.add_argument("--joint-race", action="store_true")
    parser.add_argument("--joint-race-initial-pace", action="store_true")
    parser.add_argument("--joint-race-delay-ms", type=int, choices=(0, 50), default=0)
    parser.add_argument("--joint-wall-pace-ms", type=int, choices=(0, 1), default=1)
    parser.add_argument("--joint-fault", choices=("none", "analog", "timeout", "disconnect", "backend"), default="none")
    parser.add_argument("--guarded", action="store_true")
    parser.add_argument("--reject-action", choices=("none", "reset", "interrupt", "detach"), default="none")
    parser.add_argument("--fault", choices=("none", "backend", "timeout"), default="none")
    parser.add_argument("--ide-timeout", type=int, default=150, help="IDE wall-clock deadline, including startup dialogs")
    args = parser.parse_args()
    e05.check(150 <= args.ide_timeout <= 900, "IDE timeout must be between 150 and 900 seconds")
    e05.check(not args.guarded or args.profile != "pause", "Guarded pause is a rejected action, not a supported profile")
    e05.check(args.reject_action == "none" or (args.guarded and args.profile == "normal"),
              "Rejection tests require the guarded normal profile")
    e05.check(args.fault == "none" or (args.guarded and args.profile == "normal" and args.reject_action == "none"),
              "Fault tests require guarded normal profile without a rejected action")
    e05.check((args.profile in ("cooperative", "joint")) == (args.experimental_renode is not None), "Experimental backend requires cooperative profile")
    e05.check(args.profile not in ("cooperative", "joint") or args.guarded, "Cooperative profile requires its protected endpoint")
    e05.check(args.joint_fault == "none" or args.profile == "joint", "Joint fault requires the joint profile")
    e05.check(not args.joint_lifecycle or (args.profile == "joint" and args.joint_fault == "none"), "Joint lifecycle requires joint profile without fault injection")
    e05.check(args.joint_wall_pace_ms == 1 or (args.profile == "joint" and (args.joint_fault == "none" or (args.joint_race and args.joint_fault in ("analog", "timeout", "backend", "disconnect"))) and not args.joint_lifecycle),
              "Unpaced characterization requires a single joint session without injected faults")
    e05.check(not args.joint_breakpoint_first or (args.profile == "joint" and args.joint_fault == "none" and not args.joint_lifecycle and args.joint_wall_pace_ms == 0),
              "Breakpoint-first requires one unpaced joint session without faults")
    e05.check(not args.joint_race or (args.profile == "joint" and args.joint_fault in ("none", "analog", "timeout", "backend", "disconnect") and not args.joint_lifecycle and not args.joint_breakpoint_first),
              "Race arbitration requires a supported single-session fault profile")
    e05.check(args.joint_race_delay_ms == 0 or (args.joint_race and args.joint_wall_pace_ms == 0), "Race delay requires unpaced arbitration")
    e05.check(not args.joint_race_initial_pace or (args.joint_race and args.joint_race_delay_ms == 50),
              "Initial pacing requires delayed arbitration")
    e05.check(not (args.joint_race and args.joint_fault != "none") or args.joint_race_delay_ms == 50,
              "Race fault requires the delayed breakpoint observation")
    e05.check(not args.joint_steps or (args.profile == "joint" and not args.joint_breakpoint_first
              and (args.joint_wall_pace_ms == 1 or (args.joint_race and args.joint_race_delay_ms == 50))),
              "Persistent steps require paced execution or delayed breakpoint arbitration")
    ide, output = args.ide.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    plugins = ide / "plugins"
    ini = (ide / "stm32cubeide.ini").read_text(encoding="utf-8").splitlines()
    javac = ide / ini[ini.index("-vm") + 1] / "javac.exe"
    gdbs = sorted(plugins.glob("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/arm-none-eabi-gdb.exe"))
    e05.check(len(gdbs) == 1, "Select an unambiguous bundled GDB installation")
    source = HERE / "cubeide"
    classes = output / "classes"
    classes.mkdir()
    compile_command = [str(javac), "-proc:none", "-encoding", "UTF-8", "-cp", str(plugins / "*"),
                       "-d", str(classes), *map(str, sorted((source / "src").rglob("*.java")))]
    subprocess.run(compile_command, check=True, timeout=60)
    bundle = output / "org.simnodus.e05.runner_0.1.0.jar"
    with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as archive:
        for relative in ("META-INF/MANIFEST.MF", "plugin.xml"):
            archive.write(source / relative, relative)
        for path in sorted(classes.rglob("*.class")):
            archive.write(path, path.relative_to(classes).as_posix())
    config = output / "configuration"
    simple = config / "org.eclipse.equinox.simpleconfigurator"
    simple.mkdir(parents=True)
    settings = config / ".settings"
    settings.mkdir()
    startup_preferences = settings / "org.eclipse.ui.prefs"
    startup_preferences.write_text("eclipse.preferences.version=1\nwindows.defender.startup.check.skip=true\n", encoding="utf-8")
    # Copy configuration text only. Never reuse or remove an OSGi cache.
    (config / "config.ini").write_bytes((ide / "configuration/config.ini").read_bytes())
    lines = (ide / "configuration/org.eclipse.equinox.simpleconfigurator/bundles.info").read_text(encoding="utf-8").splitlines()
    resolved = []
    for line in lines:
        fields = line.split(",")
        if len(fields) == 5 and fields[2].startswith("plugins/"):
            fields[2] = (ide / fields[2]).as_uri()
            line = ",".join(fields)
        resolved.append(line)
    resolved.append(f"org.simnodus.e05.runner,0.1.0,{bundle.as_uri()},4,false")
    (simple / "bundles.info").write_text("\n".join(resolved) + "\n", encoding="utf-8")
    documentation_versions = [line.split(",")[1] for line in resolved
                              if line.startswith("com.st.stm32cube.ide.documentation,")]
    e05.check(len(documentation_versions) == 1, "Documentation bundle version is ambiguous")
    release_notes_key = ".".join(documentation_versions[0].split(".")[:3])
    release_notes_preferences = settings / "com.st.stm32cube.ide.documentation.prefs"
    release_notes_preferences.write_text("eclipse.preferences.version=1\n" + release_notes_key + "=true\n", encoding="utf-8")
    build = ROOT / "build/sn016"
    renode_args = SimpleNamespace(
        prepared=ROOT / "build/sn019/generated", firmware=build / "firmware",
        renode=args.experimental_renode.resolve() if args.experimental_renode else ROOT / "build/deps/renode/renode_1.16.1-dotnet_portable")
    subprocess.run([os.sys.executable, str(ROOT / "tools/check_backend_assets.py"),
                    "--root", str(ROOT / "build/deps")], check=True, timeout=60)
    audit = output / "audit"
    audit.mkdir()
    audit_manifest = json.loads((HERE / "audit-sources.json").read_text(encoding="utf-8"))
    for entry in audit_manifest["files"]:
        shutil.copyfile(build / "audit" / entry["file"], audit / entry["file"])
    subprocess.run([os.sys.executable, str(HERE / "audit.py"), "--root", str(audit)], check=True, timeout=30)
    firmware_build = json.loads((build / "firmware/build.json").read_text(encoding="utf-8"))
    e05.check(e05.sha(build / "firmware/firmware.elf") == firmware_build["elf_sha256"], "Firmware fingerprint changed")
    for name, digest in firmware_build["sources_sha256"].items():
        e05.check(e05.sha(HERE / name) == digest, "Firmware source changed: " + name)
    coordinator = None
    guard = None
    renode = None
    process = None
    report = {"status": "failed", "complete_e05_profile": False, "profile": args.profile, "reject_action": args.reject_action, "fault": args.fault,
              "race_host_poll_ms": 1 if args.joint_race else None,
              "joint_race_initial_pace": args.joint_race_initial_pace,
              "joint_steps": args.joint_steps, "joint_race_delay_ms": args.joint_race_delay_ms, "joint_race": args.joint_race, "joint_breakpoint_first": args.joint_breakpoint_first, "joint_lifecycle": args.joint_lifecycle, "joint_fault": args.joint_fault, "joint_wall_pace_ms": args.joint_wall_pace_ms, "started_utc": datetime.now(timezone.utc).isoformat(),
              "inputs_sha256": {path.name: e05.sha(path) for path in [
                  Path(__file__), HERE / "run.py", HERE / "loopback_gdb.cs",
                  HERE / "stm32f103c8-debug.repl", build / "firmware/firmware.elf",
                  build / "native-vs/Debug/e05_control.exe", build / "native-vs/Debug/e05_circuit.exe",
                  gdbs[0], ide / "stm32cubeidec.exe", ide / "stm32cubeide.ini",
                  ROOT / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll",
                  renode_args.renode / "Renode.exe"]},
              "startup_preferences_sha256": e05.sha(startup_preferences),
              "release_notes_preferences_sha256": e05.sha(release_notes_preferences),
              "release_notes_seen_version": release_notes_key,
              "ide": str(ide), "sources_sha256": {
                  path.relative_to(source).as_posix(): e05.sha(path)
                  for path in sorted(source.rglob("*")) if path.is_file()}}
    log_path = output / "cubeide-console.log"
    try:
        renode = e05.RenodeProcess(renode_args, output / "renode")
        report["listeners"] = renode.listeners
        if args.guarded:
            guard = CooperativeGuard(renode.debug_port) if args.profile in ("cooperative", "joint") else Guard(renode.debug_port)
        if args.profile in ("cooperative", "joint"):
            coordinator = (JointCoordinator(renode, guard, output, HERE / "cancel_notify_bridge.cs", args.joint_fault, args.joint_wall_pace_ms, args.joint_breakpoint_first, args.joint_race, args.joint_race_delay_ms, args.joint_steps, args.joint_race_initial_pace)
                           if args.profile == "joint" else Coordinator(renode, guard, output, HERE / "cancel_notify_bridge.cs"))
            report["cooperative_inputs_sha256"] = {p.name: e05.sha(p) for p in (HERE / "cooperative_debug.py", HERE / "cancel_notify_bridge.cs", renode_args.renode / "Renode.dll", renode_args.renode / "Infrastructure.dll")}
        if args.profile == "joint":
            report["joint_inputs_sha256"] = {p.name: e05.sha(p) for p in (
                HERE / "joint_ide.py", HERE / "joint_persistent.py", HERE / "joint_circuit.cpp", HERE / "cancel_joint_bridge.cs",
                build / "persistent-native/Debug/e05_joint_circuit.exe",
                build / "persistent-native/Debug/e05_control.exe")}
        values = {
            "simnodus-output": output / "runner.txt", "elf": renode_args.firmware / "firmware.elf",
            "gdb": gdbs[0], "port": guard.port if guard else renode.debug_port,
            "control": build / "native-vs/Debug/e05_control.exe",
            "control-port": renode.control_port,
            "circuit": build / "native-vs/Debug/e05_circuit.exe",
            "ngspice-dll": ROOT / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll",
            "audio": ROOT / "build/deps/ngspice-console/Spice64/bin",
            "initialization": ROOT / "tools/backend_probe/initialization",
            "renode-stopped": output / "renode-stopped",
            "disconnect-checked": output / "disconnect-checked", "profile": args.profile,
            "guarded": str(args.guarded).lower(), "guard-ready": output / "guard-ready",
            "reset-properties": output / "reset.properties", "reject-action": args.reject_action,
            "cooperative-directory": output, "joint-steps": str(args.joint_steps).lower(), "joint-race": str(args.joint_race).lower(), "joint-breakpoint-first": str(args.joint_breakpoint_first).lower(), "joint-fault": args.joint_fault, "joint-lifecycle": str(args.joint_lifecycle).lower(),
            "guard-rejection": output / "guard-rejection.txt", "fault": args.fault,
        }
        properties = output / "launch.properties"
        properties.write_text("".join(name + "=" + str(value).replace("\\", "\\\\") + "\n"
                                      for name, value in values.items()), encoding="utf-8")
        environment = os.environ.copy()
        environment["SIMNODUS_E05_PROPERTIES"] = str(properties)
        command = [str(ide / "stm32cubeidec.exe"), "-nosplash", "-consolelog", "-clean", "-disableAnalytics",
                   "-configuration", str(config), "-data", str(output / "workspace")]
        report["command"] = command
        with log_path.open("w", encoding="utf-8") as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                       env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
            deadline = time.monotonic() + args.ide_timeout
            while process.poll() is None and time.monotonic() < deadline:
                runner = values["simnodus-output"]
                transcript = runner.read_text(encoding="utf-8") if runner.exists() else ""
                if args.joint_lifecycle and "joint_first_session" in report:
                    transcript = transcript.partition("REQUEST_JOINT_RESET")[2]
                    coordinator.poll(transcript.partition("JOINT_SECOND_SEQUENCE")[2])
                elif coordinator:
                    coordinator.poll(transcript)
                if args.joint_lifecycle and "REQUEST_JOINT_RESET" in transcript and "joint_first_session" not in report:
                    e05.check(coordinator.phase == "complete" and renode.process.poll() == 0,
                              "Joint reset preceded a completed first session")
                    coordinator.close()
                    guard.close()
                    first_evidence = {"cooperative": coordinator.evidence, "guard": guard.evidence(),
                        "listeners_removed": not e05.sn019.listeners(renode.process.pid),
                        "guard_listener_removed": not any(item["port"] == guard.port for item in e05.sn019.listeners(os.getpid()))}
                    e05.check(first_evidence["listeners_removed"] and first_evidence["guard_listener_removed"]
                              and not guard.rejected.is_set(), "Prior joint resources survived reset")
                    report["joint_first_session"] = first_evidence
                    coordinator = None
                    second = output / "second"
                    second.mkdir()
                    renode = e05.RenodeProcess(renode_args, second / "renode")
                    guard = CooperativeGuard(renode.debug_port)
                    coordinator = JointCoordinator(renode, guard, second, HERE / "cancel_notify_bridge.cs", steps=args.joint_steps)
                    values.update({"port": guard.port, "control-port": renode.control_port,
                        "cooperative-directory": second, "renode-stopped": second / "renode-stopped",
                        "disconnect-checked": second / "disconnect-checked", "joint-lifecycle": "false"})
                    write_atomic(values["reset-properties"], "".join(name + "=" + str(value).replace("\\", "\\\\") + "\n"
                        for name, value in values.items()))
                    transcript = ""

                if args.joint_fault in ("disconnect", "backend") and coordinator.phase in ("cancelling", "backend_lost", "failed") and guard.rejected.is_set():
                    report["expected_fault"] = {"kind": "joint_" + args.joint_fault, "committed": False,
                                                "phase": coordinator.phase, **coordinator.evidence.get("failure", {})}
                fault_marker = {"backend": "REQUEST_BACKEND_FAILURE", "timeout": "REQUEST_STOP_TIMEOUT"}.get(args.fault)
                if fault_marker and fault_marker in transcript and "expected_fault" not in report:
                    e05.check(guard.armed and not guard.rejected.is_set(), "Fault preceded by a transport rejection")
                    if args.fault == "backend":
                        backend_exit = renode.terminate(normal=False)
                        e05.check(backend_exit != 0, "Injected backend failure exited normally")
                        e05.check(guard.rejected.wait(3), "Relay did not detect backend loss")
                        e05.check(guard.diagnostic["payload"] is None, "Backend loss was a command rejection")
                        reason = "backend connection lost during pending grant"
                    else:
                        match = re.search(r"STOP_TIMEOUT wallNs=(\d+) expectedPc=0x800fffc observedPc=0x8000102", transcript)
                        e05.check(match is not None and 2_000_000_000 <= int(match[1]) <= 5_000_000_000,
                                  "Missing bounded unreachable-stop timeout evidence")
                        guard.expected_exit = True
                        backend_exit = renode.terminate(normal=True)
                        reason = "expected stop deadline expired"
                    guard.close()
                    report["expected_fault"] = {"kind": args.fault, "reason": reason,
                        "backend_exit": backend_exit, "committed": False,
                        "guard_diagnostic": guard.diagnostic}
                    write_atomic(values["guard-rejection"],
                        "Session ended: " + reason + ". Start a new session to recover.")
                if guard:
                    if guard.rejected.is_set() and "expected_fault" not in report:
                        e05.check(args.reject_action != "none", "Guard rejected IDE command: " + str(guard.diagnostic))
                        if not values["guard-rejection"].exists():
                            expected = {"reset": b"qRcmd," + b"machine Reset".hex().encode(),
                                        "detach": b"D"}.get(args.reject_action)
                            diagnostic = guard.diagnostic
                            e05.check(diagnostic["forwarded"] is False and diagnostic["committed"] is False,
                                      "Guard rejection forwarded or committed state")
                            if expected is not None:
                                e05.check(diagnostic["payload"] == expected.hex(), "Unexpected rejected packet")
                            else:
                                e05.check("interrupt" in diagnostic["reason"], "Unexpected interrupt rejection")
                            guard.expected_exit = True
                            renode.terminate(normal=True)
                            guard.close()
                            write_atomic(values["guard-rejection"],
                                "Session ended: " + diagnostic["reason"] + ". Start a new session to recover.")
                            report["expected_rejection"] = diagnostic
                    if runner.exists() and "REQUEST_HOST_GRANT" in runner.read_text(encoding="utf-8") and not values["guard-ready"].exists():
                        guard.arm(5000)
                        values["guard-ready"].touch()
                if not values["renode-stopped"].exists() and renode.process.poll() is None and runner.exists() and "REQUEST_RENODE_STOP" in transcript:
                    if guard: guard.expected_exit = True
                    renode.terminate(normal=True)
                    values["renode-stopped"].touch()
                if runner.exists() and "DISCONNECTED" in runner.read_text(encoding="utf-8") and not values["disconnect-checked"].exists():
                    renode_args.native = build / "native-vs/Debug"
                    detached = e05.control(renode_args, renode, "time")
                    e05.check(detached["after_us"] == 0, "Disconnect advanced time without a grant")
                    report["disconnect_time"] = detached
                    if args.joint_lifecycle:
                        analog_zero = coordinator.analog.command("inspect")
                        e05.check(analog_zero == coordinator.evidence["initial_analog"], "Persistent circuit advanced during disconnect")
                        report["joint_disconnected_analog"] = analog_zero
                    values["disconnect-checked"].touch()
                if runner.exists() and "REQUEST_SESSION_RESET" in runner.read_text(encoding="utf-8") and not values["reset-properties"].exists():
                    e05.check(renode.process.poll() == 0, "Reset requested before prior backend shutdown")
                    report["prior_session_listeners_removed"] = not e05.sn019.listeners(renode.process.pid)
                    if guard:
                        guard.close()
                        report["prior_guard"] = guard.evidence()
                        e05.check(not guard.rejected.is_set(), "Prior guarded session failed before reset")
                        report["prior_guard_listener_removed"] = not any(item["port"] == guard.port for item in e05.sn019.listeners(os.getpid()))
                    e05.check(report["prior_session_listeners_removed"] and report.get("prior_guard_listener_removed", True),
                              "Old session listeners survived reset")
                    renode = e05.RenodeProcess(renode_args, output / "reset-renode")
                    report["reset_listeners"] = renode.listeners
                    guard = Guard(renode.debug_port) if args.guarded else None
                    values["port"] = guard.port if guard else renode.debug_port
                    values["control-port"] = renode.control_port
                    values["profile"] = "reset"
                    write_atomic(values["reset-properties"], "".join(name + "=" + str(value).replace("\\", "\\\\") + "\n"
                        for name, value in values.items()))
                active_race = (args.joint_race and coordinator.index == coordinator.pause_index
                               and coordinator.phase not in ("complete", "failed"))
                time.sleep(0.001 if active_race else 0.01 if coordinator else 0.1)
            e05.check(process.poll() is not None, "CubeIDE startup probe exceeded its wall-clock deadline")
        report["ide_exit"] = process.returncode
        shutdown = output / "cubeide-shutdown.txt"
        e05.check(shutdown.exists() and shutdown.read_text(encoding="utf-8") == "accepted=true",
                  "Workbench close did not acknowledge acceptance")
        report["workbench_close_accepted"] = True
        result = values["simnodus-output"]
        e05.check(result.exists(), "CubeIDE startup executor did not produce evidence")
        result_text = result.read_text(encoding="utf-8")
        e05.check(not (output / "cubeide-startup-failure.txt").exists(), "CubeIDE startup executor failed")
        required_marker = {"normal": "SIMNODUS_CUBEIDE_RUNNER_OK", "pause": "PAUSE observedNs=",
                           "reset": "RECONNECTED timeNs=0", "lifecycle": "SIMNODUS_LIFECYCLE_OK", "cooperative": "SIMNODUS_COOPERATIVE_IDE_OK", "joint": "SIMNODUS_JOINT_IDE_OK"}[args.profile]
        if args.joint_lifecycle: required_marker = "SIMNODUS_JOINT_LIFECYCLE_OK"
        if args.joint_fault != "none": required_marker = "SIMNODUS_JOINT_FAILURE_OK"
        if args.reject_action != "none" or args.fault != "none": required_marker = "IDE_DIAGNOSTIC Session ended:"
        e05.check(required_marker in result_text and "SIMNODUS_CUBEIDE_SESSION_CLOSED" in result_text,
                  "CubeIDE partial action probe did not finish")
        e05.check(process.returncode == 0, "CubeIDE exited unsuccessfully")
        if args.profile in ("normal", "lifecycle") and args.reject_action == "none" and args.fault == "none":
            stops = []
            for match in re.finditer(r"^STOP label=(\w+) reason=(\w+) timeNs=(\d+) pc=(0x[0-9a-f]+).* mailbox=([0-9a-f]+)$", result_text, re.M):
                label, reason, instant, pc, mailbox = match.groups()
                stops.append({"label": label, "reason": reason, "time_ns": int(instant),
                              "pc": int(pc, 16), "mailbox": mailbox})
            labels = ["gpio", "instruction", "gpioEdge", "stepTarget", "next1", "next2", "next3", "adc", "adcCommitted"]
            e05.check([item["label"] for item in stops] == labels, "Incomplete stop evidence")
            times = [item["time_ns"] for item in stops]
            e05.check(times == sorted(times) and times[0] > 0 and times[-1] < 5_000_000,
                      "Stop sequence regressed or exceeded the grant")
            circuits = [json.loads(match.group(1)) for match in
                        re.finditer(r"^CIRCUIT label=\w+ result=(.+)$", result_text, re.M)]
            e05.check(len(circuits) == len(stops), "Missing circuit checkpoints")
            for stop, circuit in zip(stops, circuits):
                e05.check(circuit["target_ns"] == stop["time_ns"] and
                          abs(circuit["actual_s"] - stop["time_ns"] * 1e-9) <= 1e-12,
                          "Circuit checkpoint did not match the observed stop")
            report["stops"] = stops
            report["circuits"] = circuits
        if args.reject_action != "none":
            e05.check("expected_rejection" in report and "REJECTED_GRANT_EXIT=" in result_text,
                      "Missing actual IDE rejection evidence")
            e05.check("committedTimeNs=" not in result_text, "Rejected action published a final commit")
        if args.fault != "none":
            e05.check("expected_fault" in report and "FAILED_GRANT_EXIT=" in result_text,
                      "Missing IDE fault/grant evidence")
            e05.check("committedTimeNs=" not in result_text, "Fault published a final commit")
        if args.joint_lifecycle:
            e05.check("joint_first_session" in report and "joint_disconnected_analog" in report,
                      "Missing persistent joint reset evidence")
            e05.check("RESET timeNs=0 mailbox=" + "00" * 32 in result_text and "RECONNECTED timeNs=0" in result_text,
                      "Joint IDE reset/reconnect evidence missing")
            e05.check(result_text.count("SIMNODUS_JOINT_IDE_OK") == 2, "Both joint sequences did not complete")
        if args.joint_steps:
            sequences = [coordinator.evidence]
            if args.joint_lifecycle:
                sequences.append(report["joint_first_session"]["cooperative"])
            for sequence in sequences:
                points = sequence["checkpoints"]
                failed = args.joint_fault != "none"
                collision = sequence.get("breakpoint_collision", False)
                count = 7 if failed else 9 if collision else 10
                e05.check(len(points) == count and sequence["steps"] and all(p["inspection_stable"] for p in points),
                          "Persistent step sequence incomplete")
                fixed = [2010875,2011000,2011375,2012125,2012250,2012500,2012875]
                if not failed:
                    fixed += [4010750,4027000]
                e05.check([p["time_ns"] for i,p in enumerate(points) if failed or collision or i != 7] == fixed,
                          "Persistent step endpoints changed")
                if not failed:
                    e05.check((collision or 2012875 < points[7]["time_ns"] < 4010750)
                              and points[7 if collision else 8]["adc_code"] == 3541,
                              "Persistent step pause or ADC differs")
                else:
                    e05.check(not any("adc_code" in p for p in points), "Failed steps transferred ADC")
            e05.check(result_text.count("JOINT_STEP_SOURCE index=") == 4 * len(sequences)
                      and result_text.count("JOINT_REGISTERS index=") == sum(len(s["checkpoints"]) for s in sequences),
                      "Persistent step source-frame evidence missing")
        if args.joint_race_initial_pace:
            e05.check(coordinator.evidence.get("pacing_released_after_retained_interrupt"),
                      "Race omitted the verified pacing release")
        if args.joint_race and args.joint_fault == "none":
            points = coordinator.evidence["checkpoints"]
            collision = coordinator.evidence.get("breakpoint_collision", False)
            e05.check(not args.joint_race_delay_ms or collision, "Injected race did not exercise breakpoint coexistence")
            e05.check(len(points) == coordinator.end_count - int(collision) and sum("adc_code" in p for p in points) == 1
                      and all(p["inspection_stable"] for p in points), "Race arbitration sequence incomplete")
            e05.check([p["adc_code"] for p in points if "adc_code" in p] == [3541], "Race ADC transfer differs")
            interrupts = [p for p in guard.evidence()["packets"] if p.get("interrupt_intercepted")]
            e05.check(len(interrupts) == 1 and not interrupts[0]["forwarded"], "Race omitted actual protected MI interrupt")
            if collision:
                e05.check("JOINT_RACE_BREAKPOINT_CONFIRMED" in result_text and points[coordinator.pause_index]["time_ns"] == 4010750
                          and not (output / "joint-notified").exists() and not (output / f"joint-ready-{coordinator.adc_index}").exists(),
                          "Race breakpoint was not preserved")
        if args.joint_race and args.joint_fault in ("analog", "timeout"):
            failure = coordinator.evidence["failure"]
            e05.check(coordinator.evidence.get("breakpoint_collision") and "JOINT_RACE_BREAKPOINT_CONFIRMED" in result_text
                      and failure["cancelled_cpu_grant"]["end"] == 4010750,
                      "Race fault did not follow the verified CPU breakpoint")
            e05.check(failure["analog_before_loss"] == coordinator.evidence["checkpoints"][-1]["analog"]
                      and not any("adc_code" in p for p in coordinator.evidence["checkpoints"]),
                      "Race loss advanced analog state or transferred ADC")
            interrupts = [p for p in guard.evidence()["packets"] if p.get("interrupt_intercepted")]
            e05.check(len(interrupts) == 1 and not interrupts[0]["forwarded"], "Race fault omitted actual interrupt")
        if args.joint_race and args.joint_fault == "backend":
            failure = coordinator.evidence["failure"]
            e05.check(failure["grant_unacknowledged"] and failure["renode_exit"] != 0
                      and failure["observed_cpu_before_loss_ns"] == 4010750
                      and "JOINT_RACE_BREAKPOINT_CONFIRMED" in result_text,
                      "Race CPU loss did not preserve the unacknowledged grant")
            e05.check(not (output / f"joint-{coordinator.pause_index}.txt").exists()
                      and failure["analog_after_loss"] == coordinator.evidence["checkpoints"][-1]["analog"],
                      "Race CPU loss advanced the joint boundary")
            packets = guard.evidence()["packets"]
            e05.check(sum(bool(p.get("interrupt_intercepted")) for p in packets) == 1
                      and sum("acknowledged_outcome" in p for p in packets) == coordinator.pause_index,
                      "Race CPU loss fabricated an acknowledgement or omitted interruption")
        if args.joint_race and args.joint_fault == "disconnect":
            failure = coordinator.evidence["failure"]
            e05.check(coordinator.evidence.get("breakpoint_collision")
                      and coordinator.evidence["disconnect_detected"] == {"cpu_backend_alive": True, "grant_armed": True, "interrupt_pending": True}
                      and failure["cancelled_cpu_grant"]["end"] == 4010750,
                      "Debugger loss did not cancel the pending arbitration grant")
            e05.check(failure["analog_after_loss"] == coordinator.evidence["checkpoints"][-1]["analog"]
                      and "JOINT_RACE_BREAKPOINT_CONFIRMED" in result_text
                      and "REQUEST_JOINT_DISCONNECT pid=" in result_text,
                      "Race disconnect evidence incomplete")
            packets = guard.evidence()["packets"]
            e05.check(sum(bool(p.get("interrupt_intercepted")) for p in packets) == 1
                      and sum("acknowledged_outcome" in p for p in packets) == coordinator.pause_index + 1,
                      "Race disconnect omitted real interruption or cancellation")
        if args.joint_breakpoint_first:
            points = coordinator.evidence["checkpoints"]
            e05.check([p["time_ns"] for p in points] == [2010875, 2011375, 4010750, 4027000]
                      and sum("adc_code" in p for p in points) == 1 and points[2]["adc_code"] == 3541
                      and all(p["inspection_stable"] for p in points), "Breakpoint-first joint sequence incomplete")
            e05.check(not (output / "joint-notified").exists() and not (output / "joint-ready-3").exists()
                      and not any(p.get("interrupt_intercepted") for p in guard.evidence()["packets"]),
                      "Breakpoint-first duplicated a boundary or fabricated an interrupt")
            e05.check("JOINT_INSPECTED_2 timeNs=4010750 pc=134218036 reason=BREAKPOINT" in result_text
                      and coordinator.evidence["pause_wall_ns"] <= 2_000_000_000,
                      "Breakpoint-first reason or acknowledgement deadline failed")
        if coordinator:
            if args.joint_fault != "none":
                e05.check(coordinator.phase == "failed" and len(coordinator.evidence["checkpoints"]) == coordinator.pause_index,
                          "Joint fault did not stop before the pending checkpoint")
                e05.check(not (output / f"joint-stopped-{coordinator.pause_index}").exists() and not (output / "joint-notified").exists()
                          and not any("adc_code" in p for p in coordinator.evidence["checkpoints"]),
                          "Failed joint pause was acknowledged")
                diagnosis = {"timeout": "analog acknowledgement deadline expired",
                             "analog": "analog backend lost", "disconnect": "debugger connection lost during active grant", "backend": "CPU backend lost during active grant"}[args.joint_fault]
                e05.check("IDE_DIAGNOSTIC Session ended: " + diagnosis in result_text
                          and f"JOINT_INSPECTED_{coordinator.pause_index}" not in result_text, "Joint failure diagnosis is incomplete")
                deadline_ns = 5_000_000_000 if args.joint_fault == "timeout" else 2_000_000_000
                e05.check(coordinator.evidence["failure"]["wall_ns"] <= deadline_ns,
                          "Joint failure exceeded its declared deadline")
            else:
                e05.check(coordinator.phase == "complete", "Cooperative sequence incomplete")
            report["cooperative"] = coordinator.evidence
        report["status"] = "fault_verified" if args.fault != "none" or args.joint_fault != "none" else "rejection_verified" if args.reject_action != "none" else "partial_probe_passed"
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report["error"] = str(error)
    finally:
        if isinstance(coordinator, JointCoordinator):
            try:
                coordinator.close()
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
                report["status"] = "failed"
                report["analog_cleanup_error"] = str(error)
        if coordinator:
            report["cooperative"] = {"phase": coordinator.phase, **coordinator.evidence}
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        if renode is not None:
            if guard: guard.expected_exit = True
            try:
                renode.terminate(normal=not ((args.fault == "backend" or args.joint_fault == "backend") and "expected_fault" in report))
                report["listeners_removed"] = True
            except (ValueError, OSError, subprocess.SubprocessError) as error:
                report["status"] = "failed"
                report["cleanup_error"] = str(error)
        if guard:
            guard.close()
            report["guard"] = guard.evidence()
            report["guard_sha256"] = e05.sha(HERE / "gdb_guard.py")
            report["guard_listener_removed"] = not any(item["port"] == guard.port for item in e05.sn019.listeners(os.getpid()))
            if (guard.rejected.is_set() and "expected_rejection" not in report and "expected_fault" not in report) or not report["guard_listener_removed"]:
                report["status"] = "failed"
        report["completed_utc"] = datetime.now(timezone.utc).isoformat()
        (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2), flush=True)
    return 0 if report["status"] in ("partial_probe_passed", "rejection_verified", "fault_verified") else 1


if __name__ == "__main__":
    raise SystemExit(main())
