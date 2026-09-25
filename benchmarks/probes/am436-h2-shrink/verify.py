#!/usr/bin/env python3
"""Check both directions and a truncated negative on the shrunken H2 proof."""

import gzip
import json
import subprocess
import tempfile
from pathlib import Path

from shrink import EXPECTED, PINS, RUN, sha256


def run(*args):
    result = subprocess.run([str(arg) for arg in args], capture_output=True,
                            text=True, timeout=10, check=False)
    return {'exit': result.returncode, 'stdout': result.stdout,
            'stderr': result.stderr}


def main():
    for path, expected in EXPECTED.items():
        if sha256(path) != expected:
            raise RuntimeError(f'pin mismatch: {path}')
    cnf, proof = RUN / 'minimal.cnf', RUN / 'minimal.dsr'
    if sha256(cnf) != '6a1328d127f5aba0ebb01ab3e5f5bbb65bf7b5f27d09b514085f58393b82f73a':
        raise RuntimeError('minimal CNF changed')
    if sha256(proof) != 'd20123294529ef0c1c01c70f0a9ac196420860609e1d04e4380c60ec450e8649':
        raise RuntimeError('minimal proof changed')
    outcomes = {}
    with tempfile.TemporaryDirectory(prefix='verify-', dir=RUN) as directory:
        work = Path(directory)
        for direction in ('backward', 'forward'):
            flags = [] if direction == 'backward' else ['-f']
            for mode in ('no-option', 'delete-implied'):
                option = [] if mode == 'no-option' else ['--delete-implied-units']
                name = f'{mode}-{direction}'
                lsr = work / f'{name}.lsr'
                result = run(PINS / 'dsr-trim', *flags, *option, cnf, proof, lsr)
                outcomes[name] = result
                (RUN / f'{name}.log').write_text(result['stdout'] + result['stderr'])
                if mode == 'no-option':
                    expected_error = ('not in watch pointers' if direction == 'backward'
                                      else 'No UP contradiction for RAT clause')
                    if result['exit'] == 0 or expected_error not in result['stderr']:
                        raise RuntimeError(f'{name} changed rejection class')
                    verbose = run(PINS / 'dsr-trim', *flags,
                                  '-v' if direction == 'backward' else '-V',
                                  cnf, proof, work / f'{name}-verbose.lsr')
                    outcomes[f'{name}-verbose'] = verbose
                    output = verbose['stdout'] + verbose['stderr']
                    if direction == 'forward':
                        with gzip.open(RUN / f'{name}-verbose.log.gz', 'wt') as target:
                            target.write(output)
                    else:
                        (RUN / f'{name}-verbose.log').write_text(output)
                    continue
                if result['exit'] or 's VERIFIED UNSAT' not in result['stdout']:
                    raise RuntimeError(f'{name} failed')
                text_lsr = work / f'{name}.text.lsr'
                decoded = run(PINS / 'dsr-compress', '-d', lsr, text_lsr)
                checked = run(PINS / 'srcheck', cnf, text_lsr)
                outcomes[f'{name}-decode'] = decoded
                outcomes[f'{name}-trestle'] = checked
                (RUN / f'{name}.trestle.log').write_text(
                    checked['stdout'] + checked['stderr'])
                if decoded['exit'] or checked['exit'] or \
                        's VERIFIED UNSAT' not in checked['stdout']:
                    raise RuntimeError(f'{name} elaboration failed')
        truncated = work / 'truncated.dsr'
        # Keep only the first complete line. The checker accepts EOF as an
        # end-of-line delimiter and may infer the empty clause before the
        # last explicit line, so shaving one or two tail bytes is not negative.
        assert proof.read_bytes().startswith(b'\x01\x22\x00')
        truncated.write_bytes(proof.read_bytes()[:3])
        negative = run(PINS / 'dsr-trim', '--delete-implied-units',
                       '-f', cnf, truncated, work / 'truncated.lsr')
        outcomes['truncated-forward'] = negative
        (RUN / 'truncated-forward.log').write_text(
            negative['stdout'] + negative['stderr'])
        if 's VERIFIED UNSAT' in negative['stdout']:
            raise RuntimeError('truncated proof unexpectedly verified UNSAT')
    summary = {name: {'exit': value['exit'],
                      'verified': 's VERIFIED UNSAT' in value['stdout'],
                      'first_error': next((line for line in value['stderr'].splitlines()
                                           if line.startswith(('Fatal error:', 'Error:'))), '')}
               for name, value in outcomes.items()}
    (RUN / 'verify-result.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
