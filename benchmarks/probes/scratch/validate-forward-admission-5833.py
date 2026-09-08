#!/usr/bin/env python3
"""Reapply the established fixed proof/edge oracles to new5830 objects."""
from pathlib import Path

here = Path(__file__).resolve().parent
cases = [
    ('validate-forward-unit-5801.py', {
        '20260908-5798-forward-unit/fixed-proofs': '20260908-5830-forward-admission/fixed-proofs',
        'pin-5798-forward-unit-release': 'pin-5830-forward-admission-release'}),
    ('validate-forward-unit-edges-5813.py', {
        '20260908-5813-forward-unit-edges': '20260908-5834-forward-admission-edges',
        'pin-5798-forward-unit-release': 'pin-5830-forward-admission-release',
        'runtime-forward-unit-5798': 'runtime-forward-admission-5830',
        'against5798': 'against5830'}),
]
for name, replacements in cases:
    path = here / name
    program = path.read_text()
    for old, new in replacements.items():
        assert old in program, old
        program = program.replace(old, new)
    exec(compile(program, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})
