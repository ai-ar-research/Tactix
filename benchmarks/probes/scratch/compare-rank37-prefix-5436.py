#!/usr/bin/env python3
"""Validate a full stopped stream and retain the earliest portable difference."""
import hashlib
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5436-rank37-inprocess-prefix'
m = json.loads((c / 'manifest.json').read_text())
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
def rows(p):
    with p.open() as f:
        for line in f:
            if line.strip() and not line.startswith('#'): yield line.rstrip('\n').split('\t')
if len(list((c / 'results').glob('*.json'))) != 2:
    print('5436 pending'); raise SystemExit(0)
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
prefixes = {}
for arm in ('candidate', 'kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    assert r['binary_sha256'] == m['hashes']['pins/' + arm]
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    prefix = next(rows(c / 'traces' / (arm + '.prefix.tsv')))[1:]
    assert int(prefix[0]) == m['stop_conflict']
    prefixes[arm] = prefix
count, first = 0, None
for t, k in itertools.zip_longest(rows(c / 'traces/candidate.decisions.tsv'), rows(c / 'traces/kissat.decisions.tsv')):
    if t != k:
        first = dict(candidate=t, kissat=k, matching_rows=count)
        break
    count += 1
same = prefixes['candidate'] == prefixes['kissat']
assert (first is None) == same, 'Prefix/stream disagreement requires observer diagnosis'
result = dict(clean_stops=2, stop_conflict=m['stop_conflict'], complete_prefix_identical=same,
    prefixes=prefixes, first_decision_difference=first, matching_rows=count,
    limitation='Stopped proof-on diagnostic only. No complete verdict, checked proof or acceptance timing. Differences require causal interpretation, not automatic repair.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
