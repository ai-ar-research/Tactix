#!/usr/bin/env python3
"""Require the actual native trace families; preserve5538's absent-macro failure."""
from pathlib import Path
source = Path(__file__).with_name('compare-small-prefix-5538.py').read_text()
needle = "for suffix in ('decisions', 'prefix', 'boundaries', 'macro'):"
assert needle in source
source = source.replace(needle, """for campaign in (old, c):
    native = json.loads((campaign / 'results/kissat.json').read_text())
    assert set(native['trace_sha256']) == {'kissat.decisions.tsv', 'kissat.prefix.tsv', 'kissat.boundaries.tsv'}
    assert not (campaign / 'traces/kissat.macro.tsv').exists()
for suffix in ('decisions', 'prefix', 'boundaries'):""")
exec(compile(source, __file__, 'exec'))
