"""Random instruction blocks for the differential test of the SimNodus emulator (E-07, G0a.2).

Writes an ELF for a bare Cortex-M3 with 1 MiB flash at 0x08000000 and 1 MiB SRAM
at 0x20000000. Each block

  1. loads R0-R10, R12 and LR with random values and the flags from a literal pool,
     points R11 at a 64-byte scratch area and SP at the block's result slot;
  2. runs `--length` random instructions drawn from every ARMv7-M encoding group
     (no UNPREDICTABLE forms: R11, SP and PC are never written by them, memory
     accesses stay inside the scratch area, IT blocks are well formed);
  3. pushes R0-R12 and LR, the APSR, the 16 scratch words and PRIMASK (32 words).

After the last block the program writes 0xD1FFD1FF to DONE and loops. Running the
same ELF in Renode and in the SimNodus emulator and comparing the result area
word by word is the differential test.

  python generate.py --seed 1 --blocks 2048 --out build/emu-diff/batch1.elf
"""

from __future__ import annotations

import argparse
import random
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from build_firmware import find_compiler  # noqa: E402

RESULT_WORDS = 32
RESULTS = 0x20010000      # result slots, 128 bytes per block
SCRATCH = 0x20000100      # 64 bytes, shared by all blocks
DONE = 0x20000000
GENERAL = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14]  # writable registers
LOW = [0, 1, 2, 3, 4, 5, 6, 7]
CONDS = ["eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le"]
SHIFTS = ["lsl", "lsr", "asr", "ror"]


def reg(n: int) -> str:
    return {12: "r12", 14: "lr"}.get(n, f"r{n}")


class Gen:
    def __init__(self, rng: random.Random):
        self.r = rng

    def rd(self) -> str:
        return reg(self.r.choice(GENERAL))

    def rn(self) -> str:
        return reg(self.r.choice(GENERAL + [11]))  # R11 may be read

    def lo(self) -> str:
        return reg(self.r.choice(LOW))

    def modified_imm(self) -> int:
        kind = self.r.randrange(5)
        b = self.r.randrange(256)
        if kind == 0:
            return b
        if kind == 1:
            return (b << 16) | b
        if kind == 2:
            return (b << 24) | (b << 8)
        if kind == 3:
            return b * 0x01010101
        rot = self.r.randrange(8, 32)
        unrotated = 0x80 | self.r.randrange(128)
        return ((unrotated >> rot) | (unrotated << (32 - rot))) & 0xFFFFFFFF

    def shift(self) -> str:
        kind = self.r.randrange(6)
        if kind == 0:
            return ""
        if kind == 1:
            return ", rrx"
        t = self.r.choice(SHIFTS)
        if t == "lsl":
            return f", lsl #{self.r.randrange(1, 32)}"
        if t == "ror":
            return f", ror #{self.r.randrange(1, 32)}"
        return f", {t} #{self.r.randrange(1, 33)}"

    # One instruction (a list of lines; IT blocks and branches produce several).
    def instruction(self, label: str) -> list[str]:
        r = self.r
        group = r.randrange(20)
        if group == 16:  # calls: BL and BLX to a local leaf that returns with BX LR
            leaf = f"{label}_leaf{r.randrange(1 << 30)}"
            after = f"{leaf}_after"
            body = [f"b {after}", f"{leaf}:", f"adds {self.lo()}, #{r.randrange(256)}", "bx lr", f"{after}:"]
            if r.randrange(2):
                return body + [f"bl {leaf}"]
            target = reg(r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12]))
            return body + [f"adr.w {target}, {leaf}", f"orr {target}, {target}, #1", f"blx {target}"]
        if group == 17:  # table branches
            index = reg(r.choice([0, 1, 2, 3]))
            name = f"{label}_t{r.randrange(1 << 30)}"
            cases = [f"{name}_c{i}" for i in range(4)]
            lines = [f"and {index}, {index}, #3"]
            if r.randrange(2):
                lines += [f"tbb [pc, {index}]", f"{name}:"]
                lines += [f".byte ({c} - {name}) / 2" for c in cases]
            else:
                lines += [f"tbh [pc, {index}, lsl #1]", f"{name}:"]
                lines += [f".hword ({c} - {name}) / 2" for c in cases]
            for i, c in enumerate(cases):
                lines += [f"{c}:", f"adds {self.lo()}, #{i + 1}", f"b {name}_end"]
            return lines + [f"{name}_end:"]
        if group == 18:  # 16-bit loads and stores through a low base register
            base = reg(r.choice([4, 5, 6, 7]))
            t = self.lo()
            form = r.randrange(4)
            lines = [f"mov {base}, r11"]
            if form == 0:
                size, scale = r.choice([("", 4), ("b", 1), ("h", 2)])
                offset = r.randrange(0, 16) * scale
                op = r.choice(["ldr", "str"])
                return lines + [f"{op}{size} {t}, [{base}, #{offset}]"]
            if form == 1:
                index = reg(r.choice([0, 1, 2, 3]))
                op = r.choice(["ldr", "str", "ldrb", "strb", "ldrh", "strh", "ldrsb", "ldrsh"])
                return lines + [f"and {index}, {index}, #31", f"{op} {t}, [{base}, {index}]"]
            listing = ", ".join(reg(x) for x in sorted(r.sample([x for x in LOW if reg(x) != base], r.randrange(1, 4))))
            op = r.choice(["ldmia", "stmia"])
            return lines + [f"{op} {base}!, {{{listing}}}"]
        if group == 19:  # PRIMASK, events and hints
            return [r.choice(["cpsid i", "cpsie i", "sev", "sev\n    wfe", "nop"])]
        if group == 15:
            return [f"{r.choice(['nop', 'dmb', 'dsb', 'isb', 'yield'])}"]
        s = r.choice(["", "s"])
        if group == 0:  # data processing, modified immediate
            op = r.choice(["and", "bic", "orr", "orn", "eor", "add", "adc", "sbc", "sub", "rsb", "mov", "mvn",
                           "tst", "teq", "cmp", "cmn"])
            imm = self.modified_imm()
            if op in ("mov", "mvn"):
                return [f"{op}{s}.w {self.rd()}, #{imm}"]
            if op in ("tst", "teq", "cmp", "cmn"):
                return [f"{op}.w {self.rn()}, #{imm}"]
            return [f"{op}{s}.w {self.rd()}, {self.rn()}, #{imm}"]
        if group == 1:  # data processing, shifted register
            op = r.choice(["and", "bic", "orr", "orn", "eor", "add", "adc", "sbc", "sub", "rsb", "mov", "mvn",
                           "tst", "teq", "cmp", "cmn"])
            if op == "mov":  # MOV with a shift is written with the shift's mnemonic
                shift = self.shift()
                if not shift:
                    return [f"mov{s}.w {self.rd()}, {self.rn()}"]
                if shift == ", rrx":
                    return [f"rrx{s} {self.rd()}, {self.rn()}"]
                kind, amount = shift[2:].split(" ")
                return [f"{kind}{s}.w {self.rd()}, {self.rn()}, {amount}"]
            if op == "mvn":
                return [f"mvn{s}.w {self.rd()}, {self.rn()}{self.shift()}"]
            if op in ("tst", "teq", "cmp", "cmn"):
                return [f"{op}.w {self.rn()}, {self.rn()}{self.shift()}"]
            return [f"{op}{s}.w {self.rd()}, {self.rn()}, {self.rn()}{self.shift()}"]
        if group == 2:  # 16-bit data processing (flag-setting outside IT blocks)
            form = r.randrange(9)
            if form == 0:
                return [f"{r.choice(['adds', 'subs'])} {self.lo()}, {self.lo()}, {self.lo()}"]
            if form == 1:
                return [f"{r.choice(['adds', 'subs'])} {self.lo()}, {self.lo()}, #{r.randrange(8)}"]
            if form == 2:
                return [f"{r.choice(['movs', 'adds', 'subs'])} {self.lo()}, #{r.randrange(256)}"]
            if form == 3:
                return [f"cmp {self.lo()}, #{r.randrange(256)}"]
            if form == 4:
                op = r.choice(["ands", "eors", "adcs", "sbcs", "orrs", "bics", "mvns", "muls"])
                d = self.lo()
                return [f"{op} {d}, {self.lo()}" if op != "muls" else f"muls {d}, {self.lo()}, {d}"]
            if form == 5:
                return [f"{r.choice(['tst', 'cmp', 'cmn'])} {self.lo()}, {self.lo()}"]
            if form == 6:
                return [f"negs {self.lo()}, {self.lo()}"]
            if form == 7:
                op = r.choice(["lsls", "lsrs", "asrs"])
                lo = 0 if op == "lsls" else 1
                return [f"{op} {self.lo()}, {self.lo()}, #{r.randrange(lo, 32)}"]
            return [f"{r.choice(['add', 'mov'])} {self.rd()}, {self.rn()}"]  # high registers, no flags
        if group == 3:  # shifts by register
            op = r.choice(["lsl", "lsr", "asr", "ror"])
            if r.randrange(2):
                d = self.lo()
                return [f"{op}s {d}, {self.lo()}"]
            return [f"{op}{s}.w {self.rd()}, {self.rn()}, {self.rn()}"]
        if group == 4:  # multiply and divide
            op = r.choice(["mul", "mla", "mls", "umull", "smull", "umlal", "smlal", "udiv", "sdiv"])
            if op in ("umull", "smull", "umlal", "smlal"):
                lo_, hi = r.sample(GENERAL, 2)
                return [f"{op} {reg(lo_)}, {reg(hi)}, {self.rn()}, {self.rn()}"]
            if op in ("mla", "mls"):
                return [f"{op} {self.rd()}, {self.rn()}, {self.rn()}, {self.rn()}"]
            if op == "mul":
                return [f"mul.w {self.rd()}, {self.rn()}, {self.rn()}"]
            return [f"{op} {self.rd()}, {self.rn()}, {self.rn()}"]
        if group == 5:  # bit and byte manipulation
            op = r.choice(["clz", "rbit", "rev", "rev16", "revsh", "sxtb", "sxth", "uxtb", "uxth"])
            if op in ("sxtb", "sxth", "uxtb", "uxth") and r.randrange(2):
                return [f"{op}.w {self.rd()}, {self.rn()}, ror #{r.choice([8, 16, 24])}"]
            if op in ("rev", "rev16", "revsh", "sxtb", "sxth", "uxtb", "uxth") and r.randrange(2):
                return [f"{op} {self.lo()}, {self.lo()}"]
            return [f"{op}.w {self.rd()}, {self.rn()}" if op not in ("clz", "rbit") else f"{op} {self.rd()}, {self.rn()}"]
        if group == 6:  # saturation and bitfields
            form = r.randrange(6)
            if form == 0:
                sh = r.choice(["", f", lsl #{r.randrange(1, 32)}", f", asr #{r.randrange(1, 32)}"])
                return [f"ssat {self.rd()}, #{r.randrange(1, 33)}, {self.rn()}{sh}"]
            if form == 1:
                sh = r.choice(["", f", lsl #{r.randrange(1, 32)}", f", asr #{r.randrange(1, 32)}"])
                return [f"usat {self.rd()}, #{r.randrange(0, 32)}, {self.rn()}{sh}"]
            lsb = r.randrange(32)
            width = r.randrange(1, 33 - lsb)
            if form == 2:
                return [f"sbfx {self.rd()}, {self.rn()}, #{lsb}, #{width}"]
            if form == 3:
                return [f"ubfx {self.rd()}, {self.rn()}, #{lsb}, #{width}"]
            if form == 4:
                return [f"bfi {self.rd()}, {self.rn()}, #{lsb}, #{width}"]
            return [f"bfc {self.rd()}, #{lsb}, #{width}"]
        if group == 7:  # wide immediates
            form = r.randrange(4)
            if form == 0:
                return [f"movw {self.rd()}, #{r.randrange(65536)}"]
            if form == 1:
                return [f"movt {self.rd()}, #{r.randrange(65536)}"]
            imm = r.randrange(4096)
            return [f"{r.choice(['addw', 'subw'])} {self.rd()}, {self.rn()}, #{imm}"]
        if group in (8, 9):  # loads and stores in the scratch area (R11 base)
            size = r.choice(["", "b", "h", "sb", "sh"])
            align = {"": 4, "b": 1, "h": 2, "sb": 1, "sh": 2}[size]
            offset = r.randrange(0, 64 - align + 1)
            offset -= offset % align if r.randrange(4) else 0
            load = size in ("sb", "sh") or r.randrange(2)
            t = self.rd()
            if load:
                return [f"ldr{size}.w {t}, [r11, #{offset}]"]
            return [f"str{size}.w {t}, [r11, #{offset}]"]
        if group == 10:  # register offset, dual, multiple, exclusive
            form = r.randrange(5)
            if form == 0:
                index = reg(r.choice([0, 1, 2, 3]))
                size = r.choice(["", "b", "h", "sb", "sh"])
                shift = {"": 2, "b": 0, "h": 1, "sb": 0, "sh": 1}[size]
                op = "ldr" if size in ("sb", "sh") or r.randrange(2) else "str"
                return [f"and {index}, {index}, #{(0xF if shift == 2 else 0x1F)}",
                        f"{op}{size}.w {self.rd() if op == 'ldr' else self.rn()}, [r11, {index}, lsl #{shift}]"]
            if form == 1:
                a, b = r.sample(GENERAL, 2)
                offset = r.randrange(0, 15) * 4
                op = r.choice(["ldrd", "strd"])
                return [f"{op} {reg(a)}, {reg(b)}, [r11, #{offset}]"]
            if form == 2:
                regs = sorted(r.sample([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12], r.randrange(1, 5)))
                listing = ", ".join(reg(x) for x in regs)
                return [f"{r.choice(['ldmia', 'stmia'])}.w r11, {{{listing}}}"]
            if form == 3:
                t, status = r.sample(GENERAL, 2)
                offset = r.randrange(0, 15) * 4
                return [f"ldrex {self.rd()}, [r11, #{offset}]", f"strex {reg(status)}, {reg(t)}, [r11, #{offset}]"]
            status, t = r.sample(GENERAL, 2)
            return ["clrex", f"strex {reg(status)}, {reg(t)}, [r11]"]  # fails: the monitor is clear
        if group == 11:  # IT blocks
            cond = r.choice(CONDS)
            count = r.randrange(1, 5)
            pattern = "".join(r.choice("te") for _ in range(count - 1))
            inverse = {"eq": "ne", "ne": "eq", "cs": "cc", "cc": "cs", "mi": "pl", "pl": "mi", "vs": "vc",
                       "vc": "vs", "hi": "ls", "ls": "hi", "ge": "lt", "lt": "ge", "gt": "le", "le": "gt"}
            lines = [f"it{pattern} {cond}"]
            for i in range(count):
                c = cond if i == 0 or pattern[i - 1] == "t" else inverse[cond]
                form = r.randrange(4)
                if form == 0:
                    lines.append(f"add{c} {self.lo()}, {self.lo()}, #{r.randrange(8)}")
                elif form == 1:
                    lines.append(f"mov{c} {self.lo()}, #{r.randrange(256)}")
                elif form == 2:
                    lines.append(f"adds{c}.w {self.rd()}, {self.rn()}, #{self.modified_imm()}")
                else:
                    lines.append(f"cmp{c} {self.lo()}, {self.lo()}")
            return lines
        if group == 12:  # forward branches
            target = f"{label}_{r.randrange(1 << 30)}"
            form = r.randrange(4)
            filler = [f"adds {self.lo()}, #1"]
            if form == 0:
                return [f"b{r.choice(CONDS)} {target}"] + filler + [f"{target}:"]
            if form == 1:
                return [f"b{r.choice(CONDS)}.w {target}"] + filler + [f"{target}:"]
            if form == 2:
                return [f"{r.choice(['cbz', 'cbnz'])} {self.lo()}, {target}"] + filler + [f"{target}:"]
            return [f"b {target}"] + filler + [f"{target}:"]
        if group == 13:  # status register
            return [f"mrs {self.rd()}, apsr"] if r.randrange(2) else [f"msr apsr_nzcvq, {self.rn()}"]
        if group == 14:  # ADR and high-register forms
            if r.randrange(2):
                return [f"adr.w {self.rd()}, {label}"]
            return [f"cmp {self.rn()}, {self.rn()}"]  # 16-bit CMP with high registers
        raise AssertionError(group)


def block(gen: Gen, index: int, length: int) -> list[str]:
    r = gen.r
    label = f"b{index}"
    lines = [f"{label}:"]
    lines.append(f"    ldr r0, ={RESULTS + (index + 1) * RESULT_WORDS * 4:#x}")
    lines.append("    mov sp, r0")
    lines.append(f"    ldr r11, ={SCRATCH:#x}")
    for n in GENERAL[1:] + [0]:
        value = r.choice([r.getrandbits(32), r.randrange(-16, 16) & 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF,
                          r.getrandbits(8), 0xFFFFFFFF])
        if n == 0:
            lines.append(f"    ldr r0, ={r.getrandbits(4) << 28:#x}")
            lines.append("    msr apsr_nzcvq, r0")
        lines.append(f"    ldr {reg(n)}, ={value:#x}")
    for _ in range(length):
        lines += [f"    {line}" if not line.endswith(":") else line for line in gen.instruction(label)]
    lines.append("    push {r0-r12, lr}")
    lines.append("    mrs r0, apsr")
    lines.append("    push {r0}")
    lines.append("    ldmia r11, {r0-r7}")
    lines.append("    add r8, r11, #32")
    lines.append("    ldmia r8, {r8-r10, r12}")
    lines.append("    push {r0-r7}")
    lines.append("    add r0, r11, #48")
    lines.append("    ldmia r0, {r0-r3}")
    lines.append("    push {r0-r3, r8-r10, r12}")
    lines.append("    mrs r0, primask")
    lines.append("    push {r0}")
    lines.append("    cpsie i")
    lines.append("    b 1f")
    lines.append("    .ltorg")
    lines.append("1:")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--blocks", type=int, default=1024)
    parser.add_argument("--length", type=int, default=16)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path)
    args = parser.parse_args()
    gen = Gen(random.Random(args.seed))
    lines = [
        ".syntax unified", ".cpu cortex-m3", ".thumb",
        ".section .vectors, \"a\"", ".word 0x20100000", ".word start + 1",
        ".rept 14", ".word fault + 1", ".endr",
        ".text", ".thumb_func", "fault:", f"    ldr r0, ={DONE:#x}", "    ldr r1, =0xFA017FA0", "    str r1, [r0]",
        "    b .", ".ltorg",
        ".global start", ".thumb_func", "start:",
        f"    ldr r0, ={SCRATCH:#x}",
    ]
    rng = gen.r
    for i in range(16):
        lines.append(f"    ldr r1, ={rng.getrandbits(32):#x}")
        lines.append(f"    str r1, [r0, #{4 * i}]")
    for index in range(args.blocks):
        lines += block(gen, index, args.length)
    lines += [f"    ldr r0, ={DONE:#x}", "    ldr r1, =0xD1FFD1FF", "    str r1, [r0]", "    b .", ".ltorg"]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    source = args.out.with_suffix(".s")
    source.write_text("\n".join(lines) + "\n", encoding="utf-8")
    linker = args.out.with_suffix(".ld")
    linker.write_text(
        "ENTRY(start)\nMEMORY { FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1M }\n"
        "SECTIONS { .vectors : { KEEP(*(.vectors)) } > FLASH .text : { *(.text*) } > FLASH }\n", encoding="utf-8")
    compiler = find_compiler(args.toolchain)
    subprocess.run([str(compiler), "-mcpu=cortex-m3", "-mthumb", "-nostdlib", "-Wl,--build-id=none", "-Wl,--strip-all",
                    "-T", str(linker), str(source), "-o", str(args.out)], check=True, timeout=600)
    print(f"{args.out}: {args.blocks} blocks of {args.length} instructions, seed {args.seed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
