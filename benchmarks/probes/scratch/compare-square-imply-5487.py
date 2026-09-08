#!/usr/bin/env python3
"""Validate parent replay before inspecting selected implication reasons."""
from pathlib import Path
import importlib.util
import json

source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5487-square-implications').replace('5436 pending', '5487 pending')
exec(compile(source, __file__, 'exec'))
parent = root / 'benchmarks/runs/20260907-5471-square-bound-prefix'
assert sha(root / m['parent_campaign'] / 'summary.json') == m['prior_summary_sha256']
assert result == json.loads((parent / 'summary.json').read_text())
spec = importlib.util.spec_from_file_location('phase_validation', Path(__file__).with_name('validate-square-phase-5486.py'))
validation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validation)
validation.self_test()
def boundary_rows(p):
    return [s.split('\t') for s in p.read_text().splitlines() if not s.startswith('#')]
for arm in ('candidate', 'kissat'):
    for suffix in ('decisions.tsv', 'prefix.tsv'):
        assert sha(c / 'traces' / (arm + '.' + suffix)) == sha(parent / 'traces' / (arm + '.' + suffix))
    a = parent / 'traces' / (arm + '.boundaries.tsv')
    b = c / 'traces' / (arm + '.boundaries.tsv')
    if arm == 'candidate':
        assert sha(a) == sha(b)
    else:
        assert validation.validate_native_boundaries(boundary_rows(a), boundary_rows(b)) == 476
    data = (c / 'traces' / (arm + '.imply.tsv')).read_text()
    assert 'imply' in data and len(data.splitlines()) < 100000
print('PASS: complete parent replay, normalized boundaries and selected implication records')
