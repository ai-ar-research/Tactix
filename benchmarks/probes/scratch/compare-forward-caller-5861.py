#!/usr/bin/env python3
"""Reuse full-stream, hash-checked diagnostic comparators without rerunning solvers."""
from pathlib import Path
import sys

here = Path(__file__).resolve().parent
if sys.argv[1] == '31':
    source = here / 'compare-forward-unit-rank31-5814.py'
    code = source.read_text().replace('20260908-5810-forward-unit-rank31-prefix',
                                     '20260908-5856-forward-caller-rank31-prefix')
elif sys.argv[1] == '32':
    source = here / 'compare-forward-admission-rank32-5839.py'
    code = source.read_text().replace('20260908-5837-forward-admission-rank32-prefix',
                                     '20260908-5857-forward-caller-rank32-prefix')
else:
    raise ValueError('rank must be31 or32')
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
