#!/usr/bin/env python3
# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Random RV32IMC blocks for the differential test of experiment E-08 (gate G8a.2).

Writes an ELF for QEMU's `virt` board and SimNodus's RISC-V test machine
(the same addresses). Each block loads x1-x31 (except x2 and x8) from a
table, runs random instructions from every decoder entry that cannot trap,
then stores x1-x31 and mscratch back over the table. x2 and x8 point at the
block's 64-byte scratch area, which loads and stores use; branches and jumps
go forward inside the block. After the last block the program sends every
block's table and scratch area to the UART and ends with the test finisher,
so QEMU's serial output is the reference. The same seed gives the same ELF.

    python tests/experiments/riscv-differential/generate.py --seed 7 --blocks 512 --out build/riscv-diff/s7.elf
"""
import argparse
import random
import struct
import sys
from pathlib import Path

CODE_BASE = 0x80000000
DATA_BASE = 0x80100000
UART = 0x10000000
FINISHER = 0x00100000
BLOCK_DATA = 192   # 128 bytes of registers (x0 slot holds mscratch), 64 of scratch
SCRATCH = 128      # x2 and x8 point here, inside the block's data
RESERVED = {0, 2, 8}
FREE = [r for r in range(32) if r not in RESERVED]
FREE_SHORT = [r for r in range(9, 16)]  # x8-x15 without x8


def r_type(f7, rs2, rs1, f3, rd, opcode=0x33):
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | opcode


def i_type(imm, rs1, f3, rd, opcode):
    return ((imm & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | opcode


def s_type(imm, rs2, rs1, f3):
    imm &= 0xFFF
    return ((imm >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((imm & 0x1F) << 7) | 0x23


def b_type(offset, rs2, rs1, f3):
    o = offset & 0x1FFF
    return (((o >> 12) & 1) << 31) | (((o >> 5) & 0x3F) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) \
        | (((o >> 1) & 0xF) << 8) | (((o >> 11) & 1) << 7) | 0x63


def u_type(imm20, rd, opcode):
    return ((imm20 & 0xFFFFF) << 12) | (rd << 7) | opcode


def j_type(offset, rd):
    o = offset & 0x1FFFFF
    return (((o >> 20) & 1) << 31) | (((o >> 1) & 0x3FF) << 21) | (((o >> 11) & 1) << 20) \
        | (((o >> 12) & 0xFF) << 12) | (rd << 7) | 0x6F


def bit(value, n):
    return (value >> n) & 1


def cj_offset(o):
    return (bit(o, 11) << 12) | (bit(o, 4) << 11) | (((o >> 8) & 3) << 9) | (bit(o, 10) << 8) \
        | (bit(o, 6) << 7) | (bit(o, 7) << 6) | (((o >> 1) & 7) << 3) | (bit(o, 5) << 2)


def cb_offset(o):
    return (bit(o, 8) << 12) | (((o >> 3) & 3) << 10) | (((o >> 6) & 3) << 5) | (((o >> 1) & 3) << 3) \
        | (bit(o, 5) << 2)


def li(rd, value):
    """lui + addi (two 32-bit instructions) loading a 32-bit value."""
    value &= 0xFFFFFFFF
    lo = value & 0xFFF
    if lo >= 0x800:
        lo -= 0x1000
    hi = ((value - lo) >> 12) & 0xFFFFF
    return [(u_type(hi, rd, 0x37), 4), (i_type(lo, rd, 0, rd, 0x13), 4)]


def random_value(rng):
    special = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0xFFFFFFFE, 31, 32, 33]
    return rng.choice(special) if rng.random() < 0.15 else rng.getrandbits(32)


# ---------------------------------------------------------------------------
# One random instruction. Returns a list of items: (word, size) for plain
# instructions, or ("branch", builder, size, skip) for control transfers whose
# offset is filled in once the block is laid out.

def gen_instruction(rng, entry):
    rd = rng.choice(FREE)
    rs1 = rng.randrange(32)
    rs2 = rng.randrange(32)
    imm12 = rng.randrange(-2048, 2048)
    short_rd = rng.choice(FREE_SHORT)
    short_rs = rng.randrange(8, 16)
    alu_r = {"add": (0, 0), "sub": (0x20, 0), "sll": (0, 1), "slt": (0, 2), "sltu": (0, 3), "xor": (0, 4),
             "srl": (0, 5), "sra": (0x20, 5), "or": (0, 6), "and": (0, 7), "mul": (1, 0), "mulh": (1, 1),
             "mulhsu": (1, 2), "mulhu": (1, 3), "div": (1, 4), "divu": (1, 5), "rem": (1, 6), "remu": (1, 7)}
    alu_i = {"addi": 0, "slti": 2, "sltiu": 3, "xori": 4, "ori": 6, "andi": 7}
    if entry in alu_r:
        f7, f3 = alu_r[entry]
        return [(r_type(f7, rs2, rs1, f3, rd), 4)]
    if entry in alu_i:
        return [(i_type(imm12, rs1, alu_i[entry], rd, 0x13), 4)]
    if entry in ("slli", "srli", "srai"):
        f3 = 1 if entry == "slli" else 5
        f7 = 0x20 if entry == "srai" else 0
        return [(r_type(f7, rng.randrange(32), rs1, f3, rd, 0x13), 4)]
    if entry == "lui":
        return [(u_type(rng.getrandbits(20), rd, 0x37), 4)]
    if entry == "auipc":
        return [(u_type(rng.getrandbits(20), rd, 0x17), 4)]
    loads = {"lb": (0, 1), "lh": (1, 2), "lw": (2, 4), "lbu": (4, 1), "lhu": (5, 2)}
    if entry in loads:
        f3, size = loads[entry]
        return [(i_type(rng.randrange(0, 64, size), rng.choice([2, 8]), f3, rd, 0x03), 4)]
    stores = {"sb": (0, 1), "sh": (1, 2), "sw": (2, 4)}
    if entry in stores:
        f3, size = stores[entry]
        return [(s_type(rng.randrange(0, 64, size), rs2, rng.choice([2, 8]), f3), 4)]
    if entry == "fence":
        return [(0x0FF0000F, 4)]
    if entry == "fence.i":
        return [(0x0000100F, 4)]
    if entry.startswith("csrr"):
        kinds = {"csrrw": 1, "csrrs": 2, "csrrc": 3, "csrrwi": 5, "csrrsi": 6, "csrrci": 7}
        source = rng.randrange(32)  # a register, or the 5-bit immediate
        return [(i_type(0x340, source, kinds[entry], rd, 0x73), 4)]  # mscratch only
    branches = {"beq": 0, "bne": 1, "blt": 4, "bge": 5, "bltu": 6, "bgeu": 7}
    if entry in branches:
        f3 = branches[entry]
        return [("branch", lambda o, a=rs1, b=rs2, f=f3: b_type(o, b, a, f), 4, rng.randint(1, 3))]
    if entry == "jal":
        return [("branch", lambda o, d=rd: j_type(o, d), 4, rng.randint(1, 3))]
    if entry == "jalr":
        # auipc rX, 0; jalr rd, offset(rX) with the offset reaching a later instruction
        base = rng.choice(FREE)
        return [("jalr", base, rd, rng.randint(1, 3))]
    # Compressed.
    if entry == "c.addi4spn":
        nz = rng.randrange(4, 1024, 4)
        word = (((nz >> 4) & 3) << 11) | (((nz >> 6) & 0xF) << 7) | (bit(nz, 2) << 6) | (bit(nz, 3) << 5) \
            | ((short_rd - 8) << 2)
        return [(word, 2)]
    if entry == "c.lw":
        off = rng.randrange(0, 64, 4)
        return [((2 << 13) | (((off >> 3) & 7) << 10) | (0 << 7) | (bit(off, 2) << 6) | (bit(off, 6) << 5)
                 | ((short_rd - 8) << 2), 2)]  # rs1' = x8
    if entry == "c.sw":
        off = rng.randrange(0, 64, 4)
        return [((6 << 13) | (((off >> 3) & 7) << 10) | (0 << 7) | (bit(off, 2) << 6) | (bit(off, 6) << 5)
                 | ((short_rs - 8) << 2), 2)]
    if entry == "c.addi":
        imm = rng.randrange(-32, 32)
        return [((bit(imm, 5) << 12) | (rd << 7) | ((imm & 0x1F) << 2) | 1, 2)]
    if entry == "c.jal":
        return [("branch", lambda o: (1 << 13) | cj_offset(o) | 1, 2, rng.randint(1, 3))]
    if entry == "c.j":
        return [("branch", lambda o: (5 << 13) | cj_offset(o) | 1, 2, rng.randint(1, 3))]
    if entry == "c.li":
        imm = rng.randrange(-32, 32)
        return [((2 << 13) | (bit(imm, 5) << 12) | (rd << 7) | ((imm & 0x1F) << 2) | 1, 2)]
    if entry == "c.addi16sp":
        k = rng.choice([v for v in range(-32, 32) if v != 0])  # nzimm = 16 * k
        def addi16sp(imm):
            return (3 << 13) | (bit(imm, 9) << 12) | (2 << 7) | (bit(imm, 4) << 6) | (bit(imm, 6) << 5) \
                | (((imm >> 7) & 3) << 3) | (bit(imm, 5) << 2) | 1
        return [(addi16sp((16 * k) & 0x3FF), 2), (addi16sp((-16 * k) & 0x3FF), 2)]  # sp comes back
    if entry == "c.lui":
        rd_lui = rng.choice([r for r in FREE])
        imm = rng.choice([v for v in range(-32, 32) if v != 0])
        return [((3 << 13) | (bit(imm, 5) << 12) | (rd_lui << 7) | ((imm & 0x1F) << 2) | 1, 2)]
    if entry in ("c.srli", "c.srai", "c.andi"):
        kind = {"c.srli": 0, "c.srai": 1, "c.andi": 2}[entry]
        imm = rng.randrange(0, 32) if kind < 2 else rng.randrange(-32, 32)
        return [((4 << 13) | (bit(imm, 5) << 12) | (kind << 10) | ((short_rd - 8) << 7) | ((imm & 0x1F) << 2) | 1, 2)]
    if entry in ("c.sub", "c.xor", "c.or", "c.and"):
        f = {"c.sub": 0, "c.xor": 1, "c.or": 2, "c.and": 3}[entry]
        return [((4 << 13) | (3 << 10) | ((short_rd - 8) << 7) | (f << 5) | ((short_rs - 8) << 2) | 1, 2)]
    if entry in ("c.beqz", "c.bnez"):
        f3 = 6 if entry == "c.beqz" else 7
        return [("branch", lambda o, s=short_rs, f=f3: (f << 13) | cb_offset(o) | ((s - 8) << 7) | 1, 2,
                 rng.randint(1, 3))]
    if entry == "c.slli":
        sh = rng.randrange(0, 32)
        return [((rd << 7) | ((sh & 0x1F) << 2) | 2, 2)]
    if entry == "c.lwsp":
        off = rng.randrange(0, 64, 4)
        return [((2 << 13) | (bit(off, 5) << 12) | (rd << 7) | (((off >> 2) & 7) << 4) | (((off >> 6) & 3) << 2) | 2, 2)]
    if entry == "c.swsp":
        off = rng.randrange(0, 64, 4)
        return [((6 << 13) | (((off >> 2) & 0xF) << 9) | (((off >> 6) & 3) << 7) | (rs2 << 2) | 2, 2)]
    if entry == "c.mv":
        return [((4 << 13) | (rd << 7) | (rng.randrange(1, 32) << 2) | 2, 2)]
    if entry == "c.add":
        return [((4 << 13) | (1 << 12) | (rd << 7) | (rng.randrange(1, 32) << 2) | 2, 2)]
    if entry in ("c.jr", "c.jalr"):
        # auipc rX, 0; addi rX, rX, distance; c.jr/c.jalr rX
        return [("cjr", rng.choice(FREE), entry == "c.jalr", rng.randint(1, 3))]
    raise ValueError(entry)


ENTRIES = ["lui", "auipc", "jal", "jalr", "beq", "bne", "blt", "bge", "bltu", "bgeu",
           "lb", "lh", "lw", "lbu", "lhu", "sb", "sh", "sw",
           "addi", "slti", "sltiu", "xori", "ori", "andi", "slli", "srli", "srai",
           "add", "sub", "sll", "slt", "sltu", "xor", "srl", "sra", "or", "and",
           "mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu",
           "fence", "fence.i", "csrrw", "csrrs", "csrrc", "csrrwi", "csrrsi", "csrrci",
           "c.addi4spn", "c.lw", "c.sw", "c.addi", "c.jal", "c.li", "c.addi16sp", "c.lui",
           "c.srli", "c.srai", "c.andi", "c.sub", "c.xor", "c.or", "c.and", "c.j", "c.beqz", "c.bnez",
           "c.slli", "c.lwsp", "c.jr", "c.mv", "c.jalr", "c.add", "c.swsp"]


def lay_out(groups):
    """Lays out a block. Each group (one generated instruction, or a sequence
    that must stay together: auipc + jalr, the c.addi16sp pair) is one unit;
    a control transfer in group k lands at the start of group k + 1 + skip,
    or at the end of the block."""
    slots = []  # (group index, kind, payload, size)
    for k, group in enumerate(groups):
        for item in group:
            if item[0] == "jalr":
                _, base, rd, skip = item
                slots.append((k, "auipc", base, 4))
                slots.append((k, "jalr", (base, rd, skip), 4))
            elif item[0] == "cjr":
                _, base, link, skip = item
                slots.append((k, "auipc", base, 4))
                slots.append((k, "cjr-addi", (base, skip), 4))
                slots.append((k, "cjr", (base, link), 2))
            elif item[0] == "branch":
                _, builder, size, skip = item
                slots.append((k, "branch", (builder, skip), size))
            else:
                slots.append((k, "word", item[0], item[1]))
    offsets = []
    group_start = {}
    at = 0
    for slot in slots:
        group_start.setdefault(slot[0], at)
        offsets.append(at)
        at += slot[3]
    end = at

    def target(k, skip):
        return group_start.get(k + 1 + skip, end)

    words = []
    for i, (k, kind, payload, size) in enumerate(slots):
        if kind == "word":
            words.append((payload, size))
        elif kind == "auipc":
            words.append((u_type(0, payload, 0x17), 4))
        elif kind == "jalr":
            base, rd, skip = payload
            words.append((i_type(target(k, skip) - offsets[i - 1], base, 0, rd, 0x67), 4))
        elif kind == "cjr-addi":
            base, skip = payload
            words.append((i_type(target(k, skip) - offsets[i - 1], base, 0, base, 0x13), 4))
        elif kind == "cjr":
            base, link = payload
            words.append(((4 << 13) | ((1 if link else 0) << 12) | (base << 7) | 2, 2))
        elif kind == "branch":
            builder, skip = payload
            words.append((builder(target(k, skip) - offsets[i]), size))
    return words


def build(seed, blocks, length):
    rng = random.Random(seed)
    code = []
    data = bytearray()
    for b in range(blocks):
        block_data = DATA_BASE + b * BLOCK_DATA
        table = [0] + [random_value(rng) for _ in range(31)]
        data += struct.pack("<32I", *table)
        data += bytes(rng.getrandbits(8) for _ in range(64))
        scratch = block_data + SCRATCH
        code += li(8, scratch)
        code.append((i_type(0, 8, 0, 2, 0x13), 4))  # addi x2, x8, 0
        for r in FREE:
            code.append((i_type(-SCRATCH + 4 * r, 8, 2, r, 0x03), 4))  # lw xr, table[r](x8)
        groups = [gen_instruction(rng, rng.choice(ENTRIES)) for _ in range(length)]
        code += lay_out(groups)
        for r in FREE:
            code.append((s_type(-SCRATCH + 4 * r, r, 2, 2), 4))  # sw xr, table[r](x2)
        code.append((i_type(0x340, 0, 2, 1, 0x73), 4))  # csrrs x1, mscratch, x0
        code.append((s_type(-SCRATCH, 1, 2, 2), 4))     # sw x1, table[0](x2)
    # Send the data to the UART, then finish.
    total = blocks * BLOCK_DATA
    code += li(5, DATA_BASE) + li(6, UART) + li(7, total)
    loop = [
        (i_type(0, 5, 4, 28, 0x03), 4),   # lbu t3, 0(t0)
        (s_type(0, 28, 6, 0), 4),         # sb t3, 0(t1)
        (i_type(1, 5, 0, 5, 0x13), 4),    # addi t0, t0, 1
        (i_type(-1, 7, 0, 7, 0x13), 4),   # addi t2, t2, -1
        (b_type(-16, 0, 7, 1), 4),        # bne t2, x0, loop
    ]
    code += loop
    code += li(5, FINISHER) + li(6, 0x5555)
    code.append((s_type(0, 6, 5, 2), 4))  # sw t1, 0(t0)
    code.append((j_type(0, 0), 4))        # j .
    text = bytearray()
    for word, size in code:
        text += struct.pack("<I" if size == 4 else "<H", word)
    return bytes(text), bytes(data)


def elf(segments, entry):
    """ELF32 RISC-V executable with one PT_LOAD per (address, bytes)."""
    header_size, ph_size = 52, 32
    offset = header_size + ph_size * len(segments)
    out = bytearray()
    out += b"\x7fELF" + bytes([1, 1, 1, 0]) + bytes(8)
    out += struct.pack("<HHIIIIIHHHHHH", 2, 243, 1, entry, header_size, 0, 0x1, header_size, ph_size,
                       len(segments), 40, 0, 0)  # e_flags: RVC
    body = bytearray()
    for address, payload in segments:
        out += struct.pack("<IIIIIIII", 1, offset + len(body), address, address, len(payload), len(payload), 7, 4)
        body += payload
    return bytes(out + body)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--blocks", type=int, default=512)
    parser.add_argument("--length", type=int, default=16)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    text, data = build(args.seed, args.blocks, args.length)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(elf([(CODE_BASE, text), (DATA_BASE, data)], CODE_BASE))
    print(f"{args.out}: {args.blocks} blocks, {len(text)} bytes of code, {len(data)} of data")
    return 0


if __name__ == "__main__":
    sys.exit(main())
