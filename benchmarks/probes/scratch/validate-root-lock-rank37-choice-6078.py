#!/usr/bin/env python3
"""Validate full1M replay before interpreting the newly selected heap."""
from pathlib import Path
source = Path(__file__).with_name('validate-rank32-choice-5890.py')
code = source.read_text()
for old, new in {
    '20260908-5883-rank32-choice-frontier': '20260908-6076-root-lock-rank37-choice',
    "row[0] == '225987' and row[4] == '116746'": "row[0] == '272979' and row[4] == '116362'",
    'through300K': 'through1M',
    'Heap snapshot follows decision61017804': 'Heap snapshot follows last-common decision76953348',
    "assert all(len(row) == 18": "assert choices[arm], 'missing selected choice state'\n    assert all(len(row) == 18",
}.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
