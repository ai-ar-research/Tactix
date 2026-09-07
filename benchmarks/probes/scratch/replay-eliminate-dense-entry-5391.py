#!/usr/bin/env python3
"""Actual old/new scheduler fixture plus the retained actual native entry check."""
from pathlib import Path
root = Path(__file__).resolve().parents[3]
source = (root / 'benchmarks/probes/scratch/replay-vivify-retry-5378.py').read_text()
for a, b in [('build/pin-5377-retry-producers', 'build/pin-5387-dense-publication'),
             ('build/pin-5378-vivify-retry', 'build/pin-5391-eliminate-dense-entry'),
             ('vivify-retry-5378.cpp', 'eliminate-dense-entry-5391.cpp'),
             ('vivify-retry-5378.c', 'dense-publication-5387.c'),
             ('vivify-retry-5378-', 'eliminate-dense-entry-5391-'),
             ('20260907-5378-vivify-retry', '20260907-5391-eliminate-dense-entry'),
             ('kissat/src/vivify.c', 'kissat/src/clause.c'),
             ('expected advice-fidelity failure', 'expected phase-entry fidelity mismatch')]:
    source = source.replace(a, b)
exec(compile(source, __file__, 'exec'), {'__file__': __file__})
