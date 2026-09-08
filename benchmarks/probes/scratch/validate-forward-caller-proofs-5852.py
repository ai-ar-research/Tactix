#!/usr/bin/env python3
"""Use unchanged independent checker contracts for old and new fixed cases."""
from pathlib import Path

here = Path(__file__).resolve().parent
cases = [
    ('validate-forward-unit-5801.py', {
        '20260908-5798-forward-unit/fixed-proofs': '20260908-5849-forward-caller/continuation-proofs',
        'pin-5798-forward-unit-release': 'pin-5849-forward-caller-release'}),
    ('validate-forward-unit-5801.py', {
        '20260908-5798-forward-unit/fixed-proofs': '20260908-5849-forward-caller/caller-proofs',
        'pin-5798-forward-unit-release/tests/test_forward_unit': 'pin-5849-forward-caller-release/tests/test_forward_caller',
        'len(records) == 16': 'len(records) == 6',
        'state_model_cases=17': 'state_model_cases=3',
        'checked_micro_proofs=16': 'checked_micro_proofs=6',
        '17 state/model cases, 8 DRAT and 8 LRAT': '3 state/model cases, 3 DRAT and 3 LRAT'}),
    ('validate-forward-unit-edges-5813.py', {
        '20260908-5813-forward-unit-edges': '20260908-5853-forward-caller-edges',
        'pin-5798-forward-unit-release': 'pin-5849-forward-caller-release',
        'runtime-forward-unit-5798': 'runtime-forward-caller-5849',
        'against5798': 'against5849'}),
]
for name, replacements in cases:
    path = here / name
    program = path.read_text()
    for old, new in replacements.items():
        assert old in program, old
        program = program.replace(old, new)
    exec(compile(program, str(path), 'exec'), {'__file__': str(path), '__name__': '__main__'})
