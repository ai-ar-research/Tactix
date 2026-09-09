#!/usr/bin/env python3
"""Freeze all50 outcomes against6579 and the unchanged fixed Kissat vector."""
from pathlib import Path
source = Path(__file__).with_name('summarize-compact-stamp-full50-6371.py')
code = source.read_text()
for before, after in {
    '20260909-6366-compact-stamp-ranks1-50': '20260909-6666-congruence-empty-ranks1-50',
    '20260909-6320-substitute-collapse-ranks1-50': '20260909-6594-material-ranks1-50',
    '2026-09-09-compact-stamp-full50-6371': '2026-09-09-congruence-empty-full50-6668',
}.items():
    assert code.count(before) == 1
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
