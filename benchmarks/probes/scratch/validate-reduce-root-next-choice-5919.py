#!/usr/bin/env python3
"""Require ordinary replay before interpreting the next current Rank32 heap."""
from pathlib import Path
source = Path(__file__).with_name('validate-rank32-choice-5890.py')
code = source.read_text()
changes = {
    '20260908-5883-rank32-choice-frontier': '20260908-5917-reduce-root-next-choice',
    "row[0] == '225987' and row[4] == '116746'": "row[0] == '271085' and row[4] == '29066'",
    'Heap snapshot follows decision61017804': 'Heap snapshot follows decision67954349',
}
for old, new in changes.items():
    assert old in code
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
