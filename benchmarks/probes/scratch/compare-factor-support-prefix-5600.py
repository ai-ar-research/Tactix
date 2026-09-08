#!/usr/bin/env python3
"""Require unchanged native parent replay, then compare the complete candidate prefix."""
from pathlib import Path
import hashlib
import json
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5597-factor-support-rank32-prefix'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
if not all((c / 'results' / (arm + '.json')).exists() for arm in ('candidate', 'kissat')):
    print('5597 pending'); raise SystemExit(0)
m = json.loads((c / 'manifest.json').read_text())
old = root / m['parent_campaign']
assert sha(old / 'summary.json') == m['parent_summary_sha256']
before = json.loads((old / 'results/kissat.json').read_text())
after = json.loads((c / 'results/kissat.json').read_text())
assert before['partial_certificate'] == after['partial_certificate']
assert before['binary_sha256'] == after['binary_sha256']
assert set(before['trace_sha256']) == set(after['trace_sha256']) == {'kissat.decisions.tsv', 'kissat.prefix.tsv', 'kissat.boundaries.tsv'}
for name, digest in before['trace_sha256'].items():
    assert sha(old / 'traces' / name) == digest == sha(c / 'traces' / name) == after['trace_sha256'][name]
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix', '20260908-5597-factor-support-rank32-prefix')
source = source.replace('5436 pending', '5597 pending')
exec(compile(source, __file__, 'exec'))
