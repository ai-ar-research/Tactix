#!/usr/bin/env python3
"""Freeze a complete full50 comparison using the unchanged checked scorer."""
from pathlib import Path

source = Path(__file__).with_name('summarize-compact-stamp-full50-6371.py')
code = source.read_text()
for before, after in {
    '20260909-6366-compact-stamp-ranks1-50': '20260910-6980-xor-presence-ranks1-50',
    '20260909-6320-substitute-collapse-ranks1-50': '20260909-6743-reduce-watermark-ranks1-50',
    '2026-09-09-compact-stamp-full50-6371': '2026-09-10-xor-presence-full50-6982',
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
