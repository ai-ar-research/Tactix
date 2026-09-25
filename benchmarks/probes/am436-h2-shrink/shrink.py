#!/usr/bin/env python3
"""Shrink H2 without changing its no-option/diagnostic checker split."""

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RUN = Path(__file__).resolve().parent
SOURCE = ROOT / 'benchmarks/probes/am423-h2-current/out/1722271/checker.cnf'
PROGRAM = ROOT / ('benchmarks/runs/20260924-am369-condition-explicit-'
                  'claude-fuzz/triage/dsr-watch/prog-replay-nox5.txt')
DRIVER = ROOT / 'benchmarks/probes/am423-h2-current/driver'
PINS = ROOT / 'benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins'
EXPECTED = {
    DRIVER: '8db25626d879770caf6481f76f702f9e7de42b4763782d3c2073068a772912bf',
    PINS / 'dsr-trim': '3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee',
    PINS / 'dsr-compress': 'a44102ce11162a4687450a4c3a640d27a49703072dd29802bd259676eb668d55',
    PINS / 'srcheck': '19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945',
    SOURCE: '4b0fa70871df4cdaf32f492a0e893e24fde02fbc78db709a118e38fb7def7baa',
}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(args):
    argv = [str(arg) for arg in args]
    try:
        return subprocess.run(argv, capture_output=True, text=True,
                              timeout=10, check=False)
    except subprocess.TimeoutExpired:
        return subprocess.CompletedProcess(argv, 124, '', 'timeout')


def render(clauses, path):
    path.write_text(f'p cnf 21 {len(clauses)}\n' + ''.join(clauses))


def trial(clauses, work, *, retain=False):
    cnf, proof = work / 'case.cnf', work / 'proof.dsr'
    lsr, text_lsr = work / 'proof.lsr', work / 'proof.text.lsr'
    render(clauses, cnf)
    solver = command([DRIVER, cnf, PROGRAM, proof, 3, 43, 'api', 369229852])
    if solver.returncode or 'final 20' not in solver.stdout or not proof.is_file():
        return None
    no_option = command([PINS / 'dsr-trim', cnf, proof, lsr])
    if no_option.returncode == 0 or 'not in watch pointers' not in no_option.stderr:
        return None
    option = command([PINS / 'dsr-trim', '--delete-implied-units',
                      cnf, proof, lsr])
    if option.returncode or 's VERIFIED UNSAT' not in option.stdout:
        return None
    decode = command([PINS / 'dsr-compress', '-d', lsr, text_lsr])
    if decode.returncode:
        return None
    trestle = command([PINS / 'srcheck', cnf, text_lsr])
    if trestle.returncode or 's VERIFIED UNSAT' not in trestle.stdout:
        return None
    result = {
        'clauses': len(clauses), 'proof_bytes': proof.stat().st_size,
        'proof_sha256': sha256(proof),
        'no_option_error': no_option.stderr.strip().splitlines()[0],
    }
    if retain:
        for name, output in [('solver.log', solver), ('no-option.log', no_option),
                             ('delete-implied.log', option),
                             ('decode.log', decode), ('trestle.log', trestle)]:
            (RUN / name).write_text(output.stdout + output.stderr)
        (RUN / 'minimal.cnf').write_bytes(cnf.read_bytes())
        (RUN / 'minimal.dsr').write_bytes(proof.read_bytes())
        (RUN / 'minimal.lsr').write_bytes(text_lsr.read_bytes())
    return result


def main():
    for path, expected in EXPECTED.items():
        actual = sha256(path)
        if actual != expected:
            raise RuntimeError(f'pin mismatch: {path}: {actual}')
    lines = SOURCE.read_text().splitlines(keepends=True)
    assert lines[0].strip() == 'p cnf 21 42'
    clauses = lines[1:]
    attempts = 0
    with tempfile.TemporaryDirectory(prefix='trial-', dir=RUN) as directory:
        work = Path(directory)
        baseline = trial(clauses, work)
        if baseline is None:
            raise RuntimeError('frozen H2 seed no longer passes the split oracle')
        granularity = 2
        while len(clauses) > 1:
            chunk = max(1, (len(clauses) + granularity - 1) // granularity)
            removed = False
            for start in range(0, len(clauses), chunk):
                candidate = clauses[:start] + clauses[start + chunk:]
                if not candidate:
                    continue
                attempts += 1
                if trial(candidate, work) is not None:
                    clauses = candidate
                    granularity = max(2, granularity - 1)
                    removed = True
                    break
            if removed:
                continue
            if granularity >= len(clauses):
                break
            granularity = min(len(clauses), granularity * 2)
        final = trial(clauses, work, retain=True)
        assert final is not None
    summary = {'baseline': baseline, 'final': final, 'attempts': attempts,
               'source': str(SOURCE.relative_to(ROOT)),
               'program_sha256': sha256(PROGRAM),
               'minimal_cnf_sha256': sha256(RUN / 'minimal.cnf'),
               'pins': {str(path.relative_to(ROOT)): digest
                        for path, digest in EXPECTED.items()}}
    (RUN / 'result.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
