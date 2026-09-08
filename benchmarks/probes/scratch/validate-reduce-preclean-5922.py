#!/usr/bin/env python3
"""Run the combined candidate's actual guidance controls and 46 proof cases."""
from pathlib import Path

here = Path(__file__).resolve().parent
source = here / 'validate-reduce-root-5903.py'
code = source.read_text()
for old, new in {
    '20260908-5900-reduce-root': '20260908-5920-reduce-preclean',
    'runtime-reduce-root-5900': 'runtime-reduce-preclean-5920',
    'pin-5900-reduce-root-release': 'pin-5920-reduce-preclean-release',
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})

source = here / 'validate-reduce-root-final-5904.py'
code = source.read_text()
for old, new in {
    '20260908-5900-reduce-root': '20260908-5920-reduce-preclean',
    '5900-reduce-root-release': '5920-reduce-preclean-release',
    '5903-reduce-root-journal': '5923-reduce-preclean-journal',
    'runtime-reduce-root-5900': 'runtime-reduce-preclean-5920',
    '20260908-5904-reduce-root-edges': '20260908-5922-reduce-preclean-edges',
    'against5900': 'against5920',
    "['applied','applied','declined']+['refuted']*6":
        "['applied','applied','declined','applied','declined','applied','declined']+['refuted']*6",
}.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
