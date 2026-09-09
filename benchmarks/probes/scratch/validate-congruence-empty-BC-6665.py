#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('validate-material-schedule-BC-6586.py')
code = source.read_text()
for before, after in {'20260909-6584-material-schedule-BC-gate': '20260909-6661-congruence-empty-BC-gate', 'summary-6586.json': 'summary-6665.json'}.items():
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
