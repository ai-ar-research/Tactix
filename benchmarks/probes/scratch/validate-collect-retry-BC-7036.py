#!/usr/bin/env python3
"""Classify all outcomes strictly, retaining changed search work as intended."""
from pathlib import Path
source = Path(__file__).with_name('validate-material-schedule-BC-6586.py')
code = source.read_text()
for before, after in {
    '20260909-6584-material-schedule-BC-gate': '20260910-7034-collect-retry-BC-gate',
    'summary-6586.json': 'summary-7036.json',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
