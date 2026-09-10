#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('submit-largest-gap-profiles-7003.py')
code = source.read_text()
for before, after in {
    '20260910-7002-largest-gap-release-profiles': '20260910-7073-rank31-bve-priority',
    "assert [r['rank'] for r in m['selection']] == [31, 35]": "assert [r['rank'] for r in m['selection']] == [31]",
}.items():
    assert code.count(before) == 1; code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
