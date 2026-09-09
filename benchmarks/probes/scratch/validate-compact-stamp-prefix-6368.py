#!/usr/bin/env python3
"""Use the tested complete-stream comparator with explicit earlier-prefix fallback."""
from pathlib import Path
source = Path(__file__).with_name('validate-collapse-prefix-6323.py')
code = source.read_text()
for before, after in (
    ('20260909-6321-collapse-rank32-prefix', '20260909-6365-compact-stamp-rank32-prefix'),
    ('do not attribute the old1368117 split to6305', 'do not attribute the old1405411 split to6357'),
):
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
