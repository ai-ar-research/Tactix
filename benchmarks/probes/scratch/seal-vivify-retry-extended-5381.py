#!/usr/bin/env python3
"""Seal complete million-conflict decision agreement, not full-state identity."""
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5381-vivify-retry-extended'
old = root / 'benchmarks/runs/20260907-5378-vivify-retry'
native = root / 'benchmarks/runs/20260907-5373-million-prefix'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''): h.update(b)
    return h.hexdigest()
m = json.loads((c / 'manifest.json').read_text())
for rel, expected in m['hashes'].items(): assert sha(c / rel) == expected
for i, arm in enumerate(('tactix', 'kissat')):
    log = (c / 'logs' / f'1669001_{i}.log').read_text()
    assert f'{arm} exit 0' in log and 'scratch_deleted True' in log
    before = (old / 'traces' / f'{arm}.decisions.tsv').read_bytes()
    with (c / 'traces' / f'{arm}.decisions.tsv').open('rb') as f: assert f.read(len(before)) == before
assert sha(c / 'traces/kissat.decisions.tsv') == sha(native / 'traces/kissat.decisions.tsv')
assert sha(c / 'traces/tactix.decisions.tsv') == sha(c / 'traces/kissat.decisions.tsv')
count = sum(1 for _ in (c / 'traces/tactix.decisions.tsv').open())
assert count == 1928902
result = dict(stopped_clean=2, identical_decisions=count, stop_conflicts=1000000,
              prior_prefix_preserved=True, native_observer_neutral=True,
              trace_sha256={p.name: sha(p) for p in (c / 'traces').iterdir()},
              conclusion='All 1928902 decisions agree through one million conflicts. Use this bounded trajectory for cost diagnosis; do not claim whole-state, whole-solve or timing parity.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(result['conclusion'])
