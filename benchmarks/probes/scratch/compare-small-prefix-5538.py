#!/usr/bin/env python3
"""Validate full native replay, prior repaired candidate prefix, then localize."""
from pathlib import Path
import hashlib
import itertools
import json

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5537-rank32-small-prefix'
old = root / 'benchmarks/runs/20260908-5479-rank32-bound-prefix'
prior = root / 'benchmarks/runs/20260908-5529-sweep-small-rank32-trace'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()
if not all((c / 'results' / f'{arm}.json').exists() for arm in ('candidate', 'kissat')):
    print('5537 pending')
    raise SystemExit(0)
for suffix in ('decisions', 'prefix', 'boundaries', 'macro'):
    name = f'kissat.{suffix}.tsv'
    r = json.loads((old / 'results/kissat.json').read_text())
    assert sha(old / 'traces' / name) == r['trace_sha256'][name]
    assert sha(c / 'traces' / name) == r['trace_sha256'][name]
r = json.loads((prior / 'results/candidate.json').read_text())
path = prior / 'traces/candidate.decisions.tsv'
assert sha(path) == r['trace_sha256'][path.name]
count = 0
with path.open() as before, (c / 'traces/candidate.decisions.tsv').open() as after:
    for line in before:
        assert line == next(after, None), 'Repair prefix changed with observer scope'
        count += 1
assert count == 18593182
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix', '20260908-5537-rank32-small-prefix')
source = source.replace('5436 pending', '5537 pending')
exec(compile(source, __file__, 'exec'))
