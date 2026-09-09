#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-material-runtime-6593.py')
code = source.read_text()
for before, after in {
    '20260909-6592-rank50-material-three-wave-runtime': '20260909-6663-rank49-congruence-empty-runtime',
    "int(row['rank'])==50": "int(row['rank'])==49",
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
