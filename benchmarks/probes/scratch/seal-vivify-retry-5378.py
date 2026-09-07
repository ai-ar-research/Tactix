#!/usr/bin/env python3
"""Validate the isolated repair's exact BVE and decision-frontier closure."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5378-vivify-retry'
old = root / 'benchmarks/runs/20260907-5374-bve-frontier'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''): h.update(b)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['hashes'].items(): assert sha(c / rel) == expected
data = {}
for i, arm in enumerate(('tactix', 'kissat')):
    log = (c / 'logs' / f'1668921_{i}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    rows = [dict(x.split('=', 1) for x in l.split('\t')[1:]) for l in (c / 'traces' / f'{arm}.bve.tsv').read_text().splitlines() if not l.startswith('#')]
    data[arm] = [{k: r[k] for k in ('external', 'stage', 'conflicts', 'ticks', 'limit', 'applied')} for r in rows if r['stage'] in ('begin', 'end')]
assert data['tactix'] == data['kissat'] and len(data['tactix']) == 4214
assert sha(c / 'traces/tactix.decisions.tsv') == sha(c / 'traces/kissat.decisions.tsv')
count = sum(1 for _ in (c / 'traces/tactix.decisions.tsv').open())
assert count == 786753
for suffix in ('decisions.tsv', 'bve.tsv', 'state.tsv', 'boundaries.tsv'):
    assert sha(c / 'traces' / ('kissat.' + suffix)) == sha(old / 'traces' / ('kissat.' + suffix))
result = dict(stopped_clean=2, native_observer_neutral=True, identical_bve_attempts=2107,
              identical_decisions=count, stop_conflict=345034,
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              conclusion='Path-specific learned-large Vivify retry advice closes every BVE candidate/outcome/tick and the old decision split. Runtime and full regression acceptance remain separate.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(result['conclusion'])
