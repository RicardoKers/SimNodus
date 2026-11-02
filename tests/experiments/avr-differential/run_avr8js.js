// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Runs every block of blocks.bin in avr8js (MIT; a test-only oracle installed
// in build/tools/avr8js) and writes avr8js.bin, the record that
// tests/unit/avr_tests.cpp compares with SimNodus's core (E-10, gate G10a.2).
//
// avr8js 0.21.1 deviates from the AVR Instruction Set Manual (DS40002198C) in
// five places, found while preparing this test. Each is corrected here in a
// copy of its instruction module, with the manual's formula, before any block
// runs; every other behaviour is avr8js's own:
//   1. H (half carry) is taken from bit 0 instead of bit 3 in ADD, ADC, SUB,
//      SUBI, SBC, SBCI, CP, CPC, CPI and NEG.
//   2. SBIW changes H; the manual says it leaves H alone.
//   3. NEG never sets V: it compares 0 - Rd (a negative number in JavaScript)
//      with 128 instead of the 8-bit result.
//   4. CBI takes one cycle; the ATmega8's datasheet and the manual's AVRe
//      column give two (avr8js's SBI has two).
//   5. FMULSU sets Z when the result is not zero (FMUL and FMULS are right).
// It also does not wrap the PC around the flash; the blocks never need that.
//
//   node run_avr8js.js <path to build/tools/avr8js/node_modules> [block to trace]
'use strict';
const fs = require('fs');
const path = require('path');

const modules = process.argv[2];
if (!modules) {
  console.error('usage: node run_avr8js.js <build/tools/avr8js/node_modules>');
  process.exit(2);
}
const avr8js = require(path.join(modules, 'avr8js'));
const version = require(path.join(modules, 'avr8js', 'package.json')).version;
if (version !== '0.21.1') {
  console.error(`avr8js ${version}: the corrections below were written for 0.21.1`);
  process.exit(1);
}

function patched() {
  const file = path.join(modules, 'avr8js', 'dist', 'cjs', 'cpu', 'instruction.js');
  let text = fs.readFileSync(file, 'utf8');
  const replace = (from, to, count) => {
    const parts = text.split(from);
    if (parts.length - 1 !== count) {
      throw new Error(`expected ${count} of ${JSON.stringify(from)}, found ${parts.length - 1}`);
    }
    text = parts.join(to);
  };
  // 2. SBIW keeps H (its block is the only one with these variable names).
  replace(
    'let sreg = cpu.data[95] & 0xc0;\n        sreg |= R ? 0 : 2;\n        sreg |= 0x8000 & R ? 4 : 0;\n        sreg |= a & ~R & 0x8000 ? 8 : 0;',
    'let sreg = cpu.data[95] & 0xe0;\n        sreg |= R ? 0 : 2;\n        sreg |= 0x8000 & R ? 4 : 0;\n        sreg |= a & ~R & 0x8000 ? 8 : 0;',
    1);
  replace('        sreg |= 1 & ((~a & l) | (l & R) | (R & ~a)) ? 0x20 : 0;\n', '', 1);
  // 3. NEG's V from the 8-bit result.
  replace('sreg |= 128 === R ? 8 : 0;', 'sreg |= 128 === (R & 255) ? 8 : 0;', 1);
  // 4. CBI: two cycles.
  replace('        cpu.writeData((A >> 3) + 32, R & ~mask, mask);\n    }',
    '        cpu.writeData((A >> 3) + 32, R & ~mask, mask);\n        cpu.cycles++;\n    }', 1);
  // 5. FMULSU's Z.
  replace('(0xffff & R ? 2 : 0)', '(0xffff & R ? 0 : 2)', 1);
  // 1. H from bit 3.
  const before = (text.match(/sreg \|= 1 & \(/g) || []).length;
  if (before !== 10) {
    throw new Error(`expected 10 half-carry lines, found ${before}`);
  }
  text = text.replace(/sreg \|= 1 & \(/g, 'sreg |= 8 & (');
  const module = { exports: {} };
  new Function('exports', 'module', text)(module.exports, module);
  return module.exports.avrInstruction;
}

const avrInstruction = patched();
// A block number as the second argument prints that block's steps (compare
// with AVR_DIFF_TRACE in avr_tests).
const trace = process.argv[3] === undefined ? -1 : Number(process.argv[3]);

const BLOCK_WORDS = 128;
const RAM_BASE = 0x100;
const RAM_BYTES = 128;
const INPUT = 32 + 1 + 2 + RAM_BYTES + 2 * BLOCK_WORDS;
const OUTPUT = 32 + 1 + 2 + 2 + 4 + 4 + RAM_BYTES;
const MAX_STEPS = 400;
const STACK_FLOOR = 0x68;

const here = __dirname;
const input = fs.readFileSync(path.join(here, 'blocks.bin'));
if (input.length % INPUT !== 0) {
  throw new Error('blocks.bin has a partial record');
}
const blocks = input.length / INPUT;
const output = Buffer.alloc(blocks * OUTPUT);
for (let b = 0; b < blocks; ++b) {
  const rec = input.subarray(b * INPUT, (b + 1) * INPUT);
  const program = new Uint16Array(4096).fill(0xffff); // erased flash, as in SimNodus
  for (let i = 0; i < BLOCK_WORDS; ++i) {
    program[i] = rec.readUInt16LE(35 + RAM_BYTES + 2 * i);
  }
  const cpu = new avr8js.CPU(program, 0x460);
  for (let r = 0; r < 32; ++r) {
    cpu.data[r] = rec[r];
  }
  cpu.data[95] = rec[32];
  cpu.SP = rec.readUInt16LE(33);
  for (let a = 0; a < RAM_BYTES; ++a) {
    cpu.data[RAM_BASE + a] = rec[35 + a];
  }
  cpu.cycles = 0;
  let steps = 0;
  // A run also ends when the stack reaches the I/O registers (what a push
  // there does is not defined the same way by the chip and by avr8js).
  while (steps < MAX_STEPS && cpu.pc < BLOCK_WORDS - 1 && cpu.SP >= STACK_FLOOR) {
    if (b === trace) {
      const hex = (v, n) => v.toString(16).toUpperCase().padStart(n, '0');
      console.log(`${String(steps).padStart(3)} pc=${hex(cpu.pc, 4)} op=${hex(program[cpu.pc] ?? 0, 4)} cyc=${cpu.cycles} ` +
        `sreg=${hex(cpu.data[95], 2)} sp=${hex(cpu.SP, 4)} r24=${hex(cpu.data[25], 2)}${hex(cpu.data[24], 2)}`);
    }
    avrInstruction(cpu);
    // The ATmega8's SPH has three bits (SP addresses 0x000-0x7FF); avr8js
    // keeps all sixteen.
    if (cpu.SP > 0x7ff) {
      cpu.SP = cpu.SP & 0x7ff;
    }
    ++steps;
  }
  const out = output.subarray(b * OUTPUT, (b + 1) * OUTPUT);
  for (let r = 0; r < 32; ++r) {
    out[r] = cpu.data[r];
  }
  out[32] = cpu.data[95];
  out.writeUInt16LE(cpu.SP & 0xffff, 33);
  out.writeUInt16LE(cpu.pc & 0xffff, 35);
  out.writeUInt32LE(cpu.cycles, 37);
  out.writeUInt32LE(steps, 41);
  for (let a = 0; a < RAM_BYTES; ++a) {
    out[45 + a] = cpu.data[RAM_BASE + a];
  }
}
fs.writeFileSync(path.join(here, 'avr8js.bin'), output);
console.log(`${blocks} blocks run in avr8js ${version}`);
