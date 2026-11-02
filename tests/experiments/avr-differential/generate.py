# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Random instruction blocks for the AVR differential test (E-10, gate G10a.2).

Writes blocks.bin: one record per block (32 registers, SREG, SP, a 128-byte
window of SRAM at 0x100, then 128 words of code). run_avr8js.js runs each
block in avr8js and writes avr8js.bin; tests/unit/avr_tests.cpp runs the same
blocks in SimNodus's core and compares.

The blocks use only the ATmega8's instructions, with operands that keep the
behaviour defined (no load into a pointer register that the instruction
moves, no stack pointer writes that the ATmega8's 11-bit SPH would cut).

    python generate.py [--blocks 1200] [--seed 2026]
"""
import argparse
import random
import struct
from pathlib import Path

BLOCK_WORDS = 128
RAM_BASE = 0x100
RAM_BYTES = 128
SP_START = 0x25F  # the middle of the SRAM (0x060-0x45F): a run seldom moves SP by 512
POS = 0  # the word the instruction being made will occupy

LOW = list(range(0, 26))  # destinations: never the X, Y, Z pointers
ANY = list(range(0, 32))
HIGH = list(range(16, 26))


def rr(base, d, r):
    return base | ((d & 0x1F) << 4) | (r & 0x0F) | ((r & 0x10) << 5)


def imm(base, d, k):
    return base | (((d - 16) & 0x0F) << 4) | (k & 0x0F) | ((k & 0xF0) << 4)


def one(base, d):
    return base | ((d & 0x1F) << 4)


def ldd(store, y, d, q):
    return 0x8000 | (0x0200 if store else 0) | (0x0008 if y else 0) | ((d & 0x1F) << 4) | (q & 7) \
        | ((q & 0x18) << 7) | ((q & 0x20) << 8)


def make(r, pos):
    """Returns a list of one or two code words for the word at `pos`."""
    global POS
    POS = pos
    kind = r.choice(OPS)
    return kind(r)


def relative(r, bits, forward=False):
    """An offset whose target stays inside the block: avr8js does not wrap the
    PC around the flash, the chip and SimNodus do. `forward`: for RCALL, so
    that a call cannot recurse until the stack runs into the I/O registers."""
    low = 0 if forward else max(-8, -(POS + 1))
    high = min(8, BLOCK_WORDS - 2 - POS)
    return r.randrange(low, high + 1) & ((1 << bits) - 1)


def two_reg(base):
    return lambda r: [rr(base, r.choice(LOW), r.choice(ANY))]


def one_reg(base):
    return lambda r: [one(base, r.choice(LOW))]


def immediate(base):
    return lambda r: [imm(base, r.choice(HIGH), r.randrange(256))]


def mul3(base):
    return lambda r: [base | ((r.randrange(16, 24) - 16) << 4) | (r.randrange(16, 24) - 16)]


def io_address(r):
    a = r.randrange(64)
    return 0x3F if a in (0x3D, 0x3E) else a  # SREG instead of SPL/SPH


OPS = [
    two_reg(0x0C00), two_reg(0x1C00), two_reg(0x1800), two_reg(0x0800),  # ADD ADC SUB SBC
    two_reg(0x1400), two_reg(0x0400), two_reg(0x1000),  # CP CPC CPSE
    two_reg(0x2000), two_reg(0x2400), two_reg(0x2800), two_reg(0x2C00), two_reg(0x9C00),  # AND EOR OR MOV MUL
    lambda r: [0x0100 | (r.randrange(13) << 4) | r.randrange(16)],  # MOVW (destination below X)
    lambda r: [0x0200 | (r.randrange(16) << 4) | r.randrange(16)],  # MULS
    mul3(0x0300), mul3(0x0308), mul3(0x0380), mul3(0x0388),  # MULSU FMUL FMULS FMULSU
    immediate(0x3000), immediate(0x4000), immediate(0x5000), immediate(0x6000), immediate(0x7000),
    immediate(0xE000),  # CPI SBCI SUBI ORI ANDI LDI
    one_reg(0x9400), one_reg(0x9401), one_reg(0x9402), one_reg(0x9403), one_reg(0x9405), one_reg(0x9406),
    one_reg(0x9407), one_reg(0x940A),  # COM NEG SWAP INC ASR LSR ROR DEC
    lambda r: [one(0x920F, r.choice(ANY))], one_reg(0x900F),  # PUSH POP
    one_reg(0x900C), one_reg(0x900D), one_reg(0x900E), one_reg(0x9009), one_reg(0x900A), one_reg(0x9001),
    one_reg(0x9002),  # LD X, X+, -X, Y+, -Y, Z+, -Z
    one_reg(0x920C), one_reg(0x920D), one_reg(0x920E), one_reg(0x9209), one_reg(0x920A), one_reg(0x9201),
    one_reg(0x9202),  # ST
    lambda r: [ldd(False, r.random() < 0.5, r.choice(LOW), r.randrange(64))],
    lambda r: [ldd(True, r.random() < 0.5, r.choice(LOW), r.randrange(64))],
    lambda r: [one(0x9000, r.choice(LOW)), RAM_BASE + r.randrange(RAM_BYTES)],  # LDS
    lambda r: [one(0x9200, r.choice(ANY)), RAM_BASE + r.randrange(RAM_BYTES)],  # STS
    lambda r: [0x95C8], one_reg(0x9004), one_reg(0x9005),  # LPM, LPM Z, LPM Z+
    lambda r: [0x9600 | (r.randrange(64) & 0x0F) | ((r.randrange(64) & 0x30) << 2)],  # ADIW r24
    lambda r: [0x9700 | (r.randrange(64) & 0x0F) | ((r.randrange(64) & 0x30) << 2)],  # SBIW r24
    lambda r: [0x9408 | (r.randrange(8) << 4)], lambda r: [0x9488 | (r.randrange(8) << 4)],  # BSET BCLR
    lambda r: [0xFA00 | (r.choice(ANY) << 4) | r.randrange(8)],  # BST
    lambda r: [0xF800 | (r.choice(LOW) << 4) | r.randrange(8)],  # BLD
    lambda r: [0xFC00 | (r.choice(ANY) << 4) | r.randrange(8)],  # SBRC
    lambda r: [0xFE00 | (r.choice(ANY) << 4) | r.randrange(8)],  # SBRS
    lambda r: [0x9800 | (r.randrange(32) << 3) | r.randrange(8)],  # CBI
    lambda r: [0x9900 | (r.randrange(32) << 3) | r.randrange(8)],  # SBIC
    lambda r: [0x9A00 | (r.randrange(32) << 3) | r.randrange(8)],  # SBI
    lambda r: [0x9B00 | (r.randrange(32) << 3) | r.randrange(8)],  # SBIS
    lambda r: [(lambda a: 0xB000 | (r.choice(LOW) << 4) | (a & 0x0F) | ((a & 0x30) << 5))(io_address(r))],  # IN
    lambda r: [(lambda a: 0xB800 | (r.choice(ANY) << 4) | (a & 0x0F) | ((a & 0x30) << 5))(io_address(r))],  # OUT
    lambda r: [0xC000 | relative(r, 12)],  # RJMP
    lambda r: [0xD000 | relative(r, 12, forward=True)],  # RCALL
    lambda r: [0xF000 | (relative(r, 7) << 3) | r.randrange(8)],  # BRBS
    lambda r: [0xF400 | (relative(r, 7) << 3) | r.randrange(8)],  # BRBC
    lambda r: [0x9508], lambda r: [0x9518],  # RET RETI
    lambda r: [0x0000],  # NOP
]

# More weight for what runs less often: LDD and STD share one entry between Y
# and Z, and PUSH competes with POP for the stack. Every operation then runs at
# least 1 000 times over the corpus (G10a.2).
def _find(mask, value):
    probe = random.Random(0)
    return next(op for op in OPS if (op(probe)[0] & mask) == value)


OPS += [_find(0xD200, 0x8000), _find(0xD200, 0x8200)] * 2 + [_find(0xFE0F, 0x920F)]


def ldi(d, k):
    return 0xE000 | ((d - 16) << 4) | (k & 0x0F) | ((k & 0xF0) << 4)


def block(r):
    regs = [r.randrange(256) for _ in range(32)]
    for low in (26, 28, 30):  # X, Y, Z inside the SRAM window
        address = RAM_BASE + r.randrange(16, RAM_BYTES - 16)
        regs[low], regs[low + 1] = address & 0xFF, address >> 8
    sreg = r.randrange(256)
    ram = bytes(r.randrange(256) for _ in range(RAM_BYTES))
    code = []
    # IJMP and ICALL through a Z that points inside the block: the jump's
    # target loads Z back into the SRAM window, so the block runs on (a random
    # Z would leave the block and end its run).
    restore = set()
    while len(code) < BLOCK_WORDS - 1:
        pos = len(code)
        if pos in restore:
            z = RAM_BASE + r.randrange(16, RAM_BYTES - 16)
            code += [ldi(30, z & 0xFF), ldi(31, z >> 8)]
            restore.discard(pos)
            continue
        if r.random() < 0.05 and pos + 6 < BLOCK_WORDS - 1:
            target = r.randrange(pos + 3, BLOCK_WORDS - 3)
            if not any(target - 2 < p < target + 2 for p in restore) and not any(pos <= p < pos + 3 for p in restore):
                code += [ldi(30, target), ldi(31, 0), r.choice([0x9409, 0x9509])]  # IJMP or ICALL
                restore.add(target)
                continue
        words = make(r, pos)
        if pos + len(words) > BLOCK_WORDS - 1 or any(pos <= p < pos + len(words) for p in restore):
            continue
        if pos == 0 and words[0] in (0x9508, 0x9518):
            continue  # a RET at 0 returns to itself, popping past the end of the SRAM
        code.extend(words)
    code.append(0x0000)  # the stop word: the run ends when the PC reaches it
    return bytes(regs) + bytes([sreg]) + struct.pack("<H", SP_START) + ram + struct.pack("<%dH" % BLOCK_WORDS, *code)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--blocks", type=int, default=1500)
    parser.add_argument("--seed", type=int, default=2026)
    args = parser.parse_args()
    r = random.Random(args.seed)
    out = Path(__file__).with_name("blocks.bin")
    out.write_bytes(b"".join(block(r) for _ in range(args.blocks)))
    print(f"{args.blocks} blocks in {out}")


if __name__ == "__main__":
    main()
