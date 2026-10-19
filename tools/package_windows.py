"""Build the portable Windows package (zip) and the installer from a configured build tree.

Steps: install the build into a staging folder, deploy the Qt runtime with
windeployqt, check that the bundled ngspice runtime is present, run the
packaged executable's --check-engine and --smoke-test (it opens windows
briefly), and write
dist/SimNodus-<version>-windows-x64.zip with a top-level SimNodus-<version>/
folder. With --installer, Inno Setup 6 also builds
dist/SimNodus-<version>-windows-x64-setup.exe from the same files
(resources/package/simnodus.iss); the setup is installed silently into a
temporary folder for the current user (no file association), the installed
program runs its checks, and the uninstaller removes it again.

usage: package_windows.py --build-dir build/release [--config Release]
                          [--windeployqt PATH] [--output dist]
                          [--installer] [--iscc PATH]
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "simnodus.exe", "simnodus-engine.exe", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "platforms/qwindows.dll",
    "ngspice/ngspice.dll", "ngspice/sndfile.dll", "ngspice/samplerate.dll", "ngspice/libomp140.x86_64.dll",
    "ngspice/spinit", "ngspice/spice2poly.cm", "ngspice/COPYING", "LICENSE", "README.txt", "THIRD-PARTY-NOTICES.txt",
    "licenses/LGPL-3.0-only.txt", "licenses/GPL-3.0-only.txt", "licenses/nlohmann-json-MIT.txt",
    "library/passive/resistor.json", "examples/01_LED_and_push_button.snx", "QUICK-START.md", "USER-GUIDE.md", "MICROCONTROLLER.md",
    "translations/simnodus_pt_BR.qm",
    "vcruntime140.dll", "msvcp140.dll",
    "library/mcu/bluepill.json", "examples/07_Blue_Pill_blink.snx", "examples/firmware/blink/blink.elf",
    "library/logic/nand.json", "library/blocks/port.json", "library/storage/counter4.json", "library/regulators/lm317.json", "library/display/decoder_4511.json",
    "examples/23_Seven_segment_counter.snx",
    "examples/22_Regulators_and_reference.snx",
    "examples/21_Counter_with_push_button.snx", "examples/20_Blocks_two_RC_stages.snx",
    "examples/blocks/RC_lowpass.snx", "examples/17_Logic_gates_half_adder.snx", "examples/08_Blue_Pill_button_interrupt.snx", "examples/firmware/button_irq/button_irq.elf",
    "examples/09_Blue_Pill_serial_terminal.snx", "examples/firmware/uart_echo/uart_echo.elf",
    "examples/10_Lesson1_GPIO_output.snx", "examples/firmware/lesson1_gpio/lesson1_gpio.elf",
    "examples/11_Lesson2_button_and_EXTI.snx", "examples/firmware/lesson2_button/lesson2_button.elf",
    "examples/lessons/lesson1_gpio_output.md", "examples/lessons/lesson2_button_exti.md",
    "examples/16_Lesson3_ADC_and_UART.snx", "examples/firmware/lesson3_adc/lesson3_adc.elf",
    "examples/lessons/lesson3_adc_uart.md",
    "examples/12_Blue_Pill_HAL_72MHz.snx", "examples/firmware/hal_clock/hal_clock.elf",
    "examples/13_Blue_Pill_timer_PWM.snx", "examples/firmware/timer_pwm/timer_pwm.elf",
    "examples/14_Blue_Pill_ADC_to_PWM.snx", "examples/firmware/adc_pwm/adc_pwm.elf",
    "examples/15_Blue_Pill_HAL_ADC_DMA.snx", "examples/firmware/hal_adc_dma/hal_adc_dma.elf",
]


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("+", " ".join(command))
    return subprocess.run(command, check=True, **kwargs)


def version(build_dir: Path) -> str:
    header = build_dir / "apps/simnodus/generated/simnodus/version.hpp"
    match = re.search(r'SIMNODUS_VERSION_FULL "([^"]+)"', header.read_text(encoding="utf-8"))
    if not match:
        raise SystemExit(f"Cannot read the version from {header}")
    return match.group(1)


def find_iscc(explicit: str | None) -> str:
    if explicit:
        return explicit
    candidates = [shutil.which("ISCC")]
    for base in (os.environ.get("LOCALAPPDATA", ""), os.environ.get("ProgramFiles(x86)", ""),
                 os.environ.get("ProgramFiles", "")):
        if base:
            candidates.append(str(Path(base) / ("Programs" if base == os.environ.get("LOCALAPPDATA") else "")
                                  / "Inno Setup 6" / "ISCC.exe"))
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise SystemExit("Inno Setup 6 (ISCC.exe) not found; install it (winget install JRSoftware.InnoSetup) "
                     "or pass --iscc")


def build_installer(iscc: str, staging: Path, output: Path, full_version: str, checks: bool) -> Path:
    numeric = re.match(r"\d+\.\d+\.\d+", full_version).group(0)
    base = f"SimNodus-{full_version}-windows-x64-setup"
    run([iscc, "/Q", f"/DAppVersion={full_version}", f"/DNumericVersion={numeric}", f"/DSourceDir={staging}",
         f"/DOutputDir={output}", f"/DOutputBaseName={base}", str(ROOT / "resources" / "package" / "simnodus.iss")])
    setup = output / f"{base}.exe"
    if checks:
        with tempfile.TemporaryDirectory() as temp:
            target = Path(temp) / "SimNodus"
            run([str(setup), "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CURRENTUSER",
                 f"/DIR={target}", "/MERGETASKS=!associate,!desktopicon", f"/LOG={Path(temp) / 'setup.log'}"],
                timeout=300)
            missing = [item for item in REQUIRED if not (target / item).exists()]
            if missing:
                raise SystemExit(f"The installer left out: {', '.join(missing)}")
            env = {k: v for k, v in os.environ.items() if k.upper() not in {"PATH", "QT_PLUGIN_PATH"}}
            env["PATH"] = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")
            engine = run([str(target / "simnodus.exe"), "--check-engine"], env=env, capture_output=True, text=True,
                         timeout=120)
            print("installed: " + engine.stdout.strip())
            smoke = run([str(target / "simnodus.exe"), "--smoke-test", "--settings", str(Path(temp) / "s.ini")],
                        env=env, capture_output=True, text=True, timeout=120)
            print("installed: " + smoke.stdout.strip().splitlines()[-1])
            uninstaller = target / "unins000.exe"
            run([str(uninstaller), "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART"], timeout=300)
            # The uninstaller removes itself shortly after it returns.
            for _ in range(50):
                if not (target / "simnodus.exe").exists():
                    break
                time.sleep(0.2)
            if (target / "simnodus.exe").exists():
                raise SystemExit("The uninstaller did not remove the program")
            print("installer checked: silent install, engine and smoke test, uninstall")
    return setup


def find_windeployqt(build_dir: Path, explicit: str | None) -> str:
    if explicit:
        return explicit
    cache = (build_dir / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^Qt6_DIR:PATH=(.+)$", cache, re.MULTILINE)
    if match:
        candidate = Path(match.group(1)).resolve().parents[2] / "bin" / "windeployqt.exe"
        if candidate.is_file():
            return str(candidate)
    found = shutil.which("windeployqt") or shutil.which("windeployqt6")
    if not found:
        raise SystemExit("windeployqt not found; pass --windeployqt")
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--config", default="Release")
    parser.add_argument("--windeployqt")
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--skip-checks", action="store_true", help="do not run the packaged executable")
    parser.add_argument("--installer", action="store_true", help="also build the Inno Setup installer")
    parser.add_argument("--iscc", help="path of Inno Setup's ISCC.exe")
    args = parser.parse_args()
    build_dir = args.build_dir.resolve()
    full_version = version(build_dir)
    name = f"SimNodus-{full_version}"
    staging_root = build_dir / "package"
    staging = staging_root / name
    if staging_root.exists():
        shutil.rmtree(staging_root)

    run(["cmake", "--install", str(build_dir), "--config", args.config, "--prefix", str(staging)])
    run([find_windeployqt(build_dir, args.windeployqt), "--release", "--no-compiler-runtime",
         "--no-opengl-sw", "--no-system-d3d-compiler", "--no-quick-import", "--translations", "pt_BR",
         str(staging / "simnodus.exe")])
    missing = [item for item in REQUIRED if not (staging / item).exists()]
    if missing:
        raise SystemExit(f"Package is incomplete, missing: {', '.join(missing)}")

    if not args.skip_checks:
        # Run with a clean PATH so that only bundled DLLs can be found.
        env = {k: v for k, v in os.environ.items() if k.upper() not in {"PATH", "QT_PLUGIN_PATH"}}
        env["PATH"] = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")
        exe = str(staging / "simnodus.exe")
        engine = run([exe, "--check-engine"], env=env, capture_output=True, text=True, timeout=120)
        print(engine.stdout.strip())
        mcu = run([exe, "--check-mcu-engine"], env=env, capture_output=True, text=True, timeout=60)
        print(mcu.stdout.strip())
        with tempfile.TemporaryDirectory() as temp:
            # The package ships only the native platform plugin, so this opens real windows briefly.
            smoke = run([exe, "--smoke-test", "--settings", str(Path(temp) / "s.ini")],
                        env=env, capture_output=True, text=True, timeout=120)
            print(smoke.stdout.strip())

    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f"{name}-windows-x64.zip"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for path in sorted(staging.rglob("*")):
            if path.is_file():
                output.write(path, path.relative_to(staging_root).as_posix())
    size = archive.stat().st_size / (1024 * 1024)
    print(f"Wrote {archive} ({size:.1f} MB)")
    if args.installer:
        setup = build_installer(find_iscc(args.iscc), staging, args.output.resolve(), full_version,
                                not args.skip_checks)
        print(f"Wrote {setup} ({setup.stat().st_size / (1024 * 1024):.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
