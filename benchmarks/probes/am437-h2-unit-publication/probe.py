#!/usr/bin/env python3
"""Checker-only unit-publication variants of the AM436 native DSR."""

import hashlib
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RUN = Path(__file__).resolve().parent
SOURCE = ROOT / 'benchmarks/probes/am436-h2-shrink'
PINS = ROOT / 'benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins'
sys.path.insert(0, str(ROOT / 'tools/bench'))
from proof_oracle import _dsr_lines  # noqa: E402

EXPECTED = {
    SOURCE / 'minimal.cnf': '6a1328d127f5aba0ebb01ab3e5f5bbb65bf7b5f27d09b514085f58393b82f73a',
    SOURCE / 'minimal.dsr': 'd20123294529ef0c1c01c70f0a9ac196420860609e1d04e4380c60ec450e8649',
    PINS / 'dsr-trim': '3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee',
    PINS / 'srcheck': '19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945',
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(*args):
    proc = subprocess.run([str(arg) for arg in args], capture_output=True,
                          text=True, timeout=10, check=False)
    return {'exit': proc.returncode, 'stdout': proc.stdout,
            'stderr': proc.stderr,
            'verified': 's VERIFIED UNSAT' in proc.stdout}


def ascii_lines():
    lines = []
    for kind, tokens in _dsr_lines(SOURCE / 'minimal.dsr', True):
        lits = [(-1 if token & 1 else 1) * (token // 2)
                for token in tokens]
        lines.append(('d ' if kind == 'd' else '') +
                     ' '.join(map(str, lits)) + (' ' if lits else '') + '0\n')
    assert len(lines) == 70 and lines[0] == '17 0\n'
    assert lines[6] == 'd -18 19 0\n'
    return lines


def main():
    for path, expected in EXPECTED.items():
        actual = sha256(path)
        if actual != expected:
            raise RuntimeError(f'pin mismatch: {path}: {actual}')
    base = ascii_lines()
    variants = {
        'baseline': base,
        'omit-reason-delete': base[:6] + base[7:],
        'unit19-omit-reason-delete': (base[:6] + ['19 0\n'] + base[7:]),
        'unit19-after-17': base[:1] + ['19 0\n'] + base[1:],
        'units18-19-after-17': base[:1] + ['18 0\n', '19 0\n'] + base[1:],
        'unit19-before-delete': base[:6] + ['19 0\n'] + base[6:],
        'units18-19-before-delete': base[:6] + ['18 0\n', '19 0\n'] + base[6:],
        'unit19-repeat-delete': (base[:6] + ['19 0\n'] + base[6:7] +
                                 ['d -18 19 0\n'] + base[7:]),
        'units18-19-repeat-delete': (base[:6] + ['18 0\n', '19 0\n'] +
                                     base[6:7] + ['d -18 19 0\n'] + base[7:]),
    }
    results = {}
    cnf = SOURCE / 'minimal.cnf'
    for name, lines in variants.items():
        dsr = RUN / f'{name}.dsr'
        dsr.write_text(''.join(lines))
        row = {'proof_sha256': sha256(dsr), 'proof_bytes': dsr.stat().st_size}
        for direction in ('backward', 'forward'):
            flags = [] if direction == 'backward' else ['-f']
            lsr = RUN / f'{name}-{direction}.lsr'
            check = command(PINS / 'dsr-trim', *flags, cnf, dsr, lsr)
            key = direction
            (RUN / f'{name}-{direction}.log').write_text(
                check['stdout'] + check['stderr'])
            row[key] = {'exit': check['exit'], 'verified': check['verified'],
                        'error': next((line for line in check['stderr'].splitlines()
                                       if line.startswith(('Error:', 'Fatal error:'))), '')}
            if check['exit'] == 0 and check['verified']:
                trestle = command(PINS / 'srcheck', cnf, lsr)
                (RUN / f'{name}-{direction}.trestle.log').write_text(
                    trestle['stdout'] + trestle['stderr'])
                row[key]['trestle'] = {'exit': trestle['exit'],
                                       'verified': trestle['verified']}
        results[name] = row
    (RUN / 'result.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
