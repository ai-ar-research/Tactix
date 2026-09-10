"""Strict streaming reader and caller-site attribution for saved perf samples."""
from dataclasses import dataclass
import re


@dataclass(frozen=True)
class Frame:
    pc: int
    symbol: str
    dso: str


@dataclass(frozen=True)
class Sample:
    period: int
    frames: tuple[Frame, ...]


def samples(lines):
    """Read `perf script -F period,ip,sym,dso`, requiring closed records."""
    period = None
    frames = []
    for raw in lines:
        line = raw.strip()
        if not line:
            if period is not None:
                if not frames:
                    raise ValueError('sample without frames')
                yield Sample(period, tuple(frames))
                period = None; frames = []
            continue
        if line.isdecimal():
            if period is not None or int(line) <= 0:
                raise ValueError('invalid or unclosed sample period')
            period = int(line)
            continue
        match = re.fullmatch(r'([0-9a-f]+)\s+(.+) \((.+)\)', line)
        if not match or period is None:
            raise ValueError('invalid frame: ' + line[:160])
        frames.append(Frame(int(match[1], 16), match[2], match[3]))
    if period is not None:
        raise ValueError('truncated final sample')


def phase(sample, binary, entries, caller=None, sites=None):
    """Return a phase and its frame index, retaining unknown callers explicitly.

`entries` maps exact entry symbols to phases; a None phase requires a direct
caller named by `caller`, with its adjusted return PC in `sites`.
"""
    found = [(i, entries[f.symbol]) for i, f in enumerate(sample.frames)
             if f.dso == binary and f.symbol in entries]
    if not found:
        return 'no_phase_frame', None
    if len(found) != 1:
        raise ValueError('nested or repeated phase entry')
    i, result = found[0]
    if result is not None:
        return result, i
    if i + 1 >= len(sample.frames):
        return 'unresolved_phase_caller', i
    outer = sample.frames[i + 1]
    if outer.dso != binary or outer.symbol != caller or outer.pc not in sites:
        return 'unresolved_phase_caller', i
    return sites[outer.pc], i


def call_sites(assembly, target, expected):
    """Bind every direct phase call to the observed perf return-address-minus-one.

The caller labels are source-reviewed input, not inferred from address order.
Require the declared x86 group argument setup immediately before each call.
"""
    instructions = []
    for line in assembly.splitlines():
        m = re.fullmatch(r'\s*([0-9a-f]+):\s+(\S.*)', line)
        if m:
            instructions.append((int(m[1], 16), ' '.join(m[2].split())))
    if not instructions or any(a[0] >= b[0] for a,b in zip(instructions, instructions[1:])):
        raise ValueError('invalid instruction inventory')
    found = {}; evidence = []
    for i, (address, instruction) in enumerate(instructions):
        if not re.fullmatch(r'callq? [0-9a-f]+ <' + re.escape(target) + r'>', instruction):
            continue
        if address not in expected or i == len(instructions)-1:
            raise ValueError('unexpected or truncated phase call')
        name, setup = expected[address]
        context = [s for _,s in instructions[max(0,i-3):i]]
        if setup not in context or instructions[i+1][0] != address + 5:
            raise ValueError('unverified argument setup or call length')
        pc = instructions[i+1][0]-1
        found[pc] = name
        evidence.append(dict(call_pc=address, adjusted_return_pc=pc,
                             phase=name, argument_setup=setup, context=context))
    if len(found) != len(expected) or len(set(found.values())) != len(found):
        raise ValueError('missing or duplicate phase call')
    return found, evidence
