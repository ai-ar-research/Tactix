#!/usr/bin/env python3
"""Validate complete parent replay before interpreting selected phase records."""
from pathlib import Path
import hashlib
import json

source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5482-square-phase').replace('5436 pending', '5482 pending')
exec(compile(source, __file__, 'exec'))
parent = root / m['parent_campaign']
assert sha(parent / 'summary.json') == m['prior_summary_sha256']
assert result == json.loads((parent / 'summary.json').read_text())
for arm in ('candidate', 'kissat'):
    for suffix in ('decisions.tsv', 'prefix.tsv', 'boundaries.tsv'):
        assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(parent / 'traces' / (arm + '.' + suffix))
    phase = c / 'traces' / (arm + '.phase.tsv')
    data = phase.read_text()
    assert 'phase_pick' in data and 'phase_save' in data
    assert len(data.splitlines()) < 100000, 'Possible observer limit truncation'
print('PASS: both complete streams/prefixes/boundaries unchanged; selected phase records present')
