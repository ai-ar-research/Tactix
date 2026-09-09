"""Strict reader for single-symbol `perf annotate --show-total-period` output.

Periods are sampled-event weights, not elapsed time or invocation counts.
Only the no-source, x86-style address/instruction format used by our runner is
accepted. Range attribution must supply reviewed instruction boundaries.
"""
from dataclasses import dataclass
import re


@dataclass(frozen=True)
class Instruction:
    address: int
    period: int
    assembly: str


@dataclass(frozen=True)
class Annotation:
    symbol: str
    samples: int
    instructions: tuple[Instruction, ...]

    @property
    def total_period(self):
        return sum(row.period for row in self.instructions)

    def range_period(self, begin, end):
        """Return a half-open instruction range, rejecting guessed boundaries."""
        addresses = {row.address for row in self.instructions}
        if begin not in addresses or end not in addresses or begin >= end:
            raise ValueError('range needs ordered exact instruction boundaries')
        return sum(row.period for row in self.instructions if begin <= row.address < end)


def parse_periods(text, expected_symbol):
    lines = text.splitlines()
    if not lines:
        raise ValueError('empty annotation')
    header = re.fullmatch(
        r'\s*Period\s*\|\s*Source code & Disassembly of .+ for cycles:u '
        r'\((\d+) samples, percent: local period\)', lines[0])
    if not header or int(header[1]) <= 0:
        raise ValueError('expected nonempty cycles:u period annotation')
    rows, symbol, start = [], None, None
    for line in lines[1:]:
        if not line.strip() or re.fullmatch(r'-+', line):
            continue
        label = re.fullmatch(r'\s*:\s*\d+\s+([0-9a-f]+) <(.+)>:', line)
        if label:
            if symbol is not None or rows or label[2] != expected_symbol:
                raise ValueError('unexpected or repeated symbol')
            start, symbol = int(label[1], 16), label[2]
            continue
        instruction = re.fullmatch(r'\s*(\d+)\s*:\s*([0-9a-f]+):\s+(\S.*)', line)
        if instruction:
            if symbol is None:
                raise ValueError('instruction before symbol')
            address = int(instruction[2], 16)
            if (rows and address <= rows[-1].address) or (not rows and address != start):
                raise ValueError('unordered or missing initial instruction')
            rows.append(Instruction(address, int(instruction[1]), instruction[3]))
            continue
        if re.fullmatch(r'\s*:\s*', line) or re.fullmatch(
                r'\s*:\s*\d+\s+Disassembly of section \.text:', line):
            continue
        raise ValueError('unrecognized annotation row: ' + line[:120])
    result = Annotation(symbol, int(header[1]), tuple(rows))
    if not rows or result.total_period <= 0:
        raise ValueError('no attributed periods')
    return result
