#!/usr/bin/env python3
"""Validate the repaired prefix extension and retain its first next split."""
import hashlib
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5424-rank35-rephase-prefix'
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
for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
if len(list((c / 'results').glob('*.json'))) != 2:
    print('5424 pending'); raise SystemExit(0)
prefixes = {}
for arm in ('candidate', 'kissat'):
    r = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
    for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
    prefix = next(rows(c / 'traces' / (arm + '.prefix.tsv')))[1:]
    assert int(prefix[0]) == 100000
    assert prefix[5:8] == ['18516218', '1', 'a6eb1df4f4e0e591'], 'Previously confirmed prefix changed'
    prefixes[arm] = prefix
equal_rows, first = 0, None
for t, k in itertools.zip_longest(rows(c / 'traces/candidate.decisions.tsv'), rows(c / 'traces/kissat.decisions.tsv')):
    if t != k:
        first = dict(candidate=t, kissat=k, matching_window_rows=equal_rows)
        break
    equal_rows += 1
same = prefixes['candidate'] == prefixes['kissat']
assert (first is None) == same, 'Full hash/stream disagreement needs observer diagnosis'
result = dict(clean_stops=2, prior_13833_prefix_preserved=True, stop_conflict=100000,
    complete_prefix_identical=same, prefixes=prefixes, first_decision_difference=first,
    matching_window_rows=equal_rows,
    limitation='Stopped proof-on diagnostic, not complete solve or timing evidence. Macro boundaries are raw localization inputs; differing counts alone do not establish semantic cause.')
p = c / 'summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
