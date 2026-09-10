#!/usr/bin/env python3
"""Align candidate attempts, retaining optional no-op flush rows as metadata."""
import importlib.util
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
reader = Path(__file__).with_name('validate-rank32-bve-candidates-6192.py')
spec = importlib.util.spec_from_file_location('frozen_bve_reader', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
c = root / 'benchmarks/runs/20260910-7018-rank35-bve-frontier'
target = c / 'attempt-alignment-7023.json'
assert not target.exists()
hashes = {}
for name in ('candidate-summary-7021.json', 'schedule-summary-7020.json'):
    p = c / name
    s = json.loads(p.read_text()); hashes[str(p.relative_to(root))] = sha(p)
    for rel, digest in s['hashes'].items():
        assert sha(root / rel) == digest, rel
        hashes[rel] = digest
for p in (Path(__file__), reader): hashes[str(p.relative_to(root))] = sha(p)
groups, omitted = {}, {}
for arm in ('candidate', 'kissat'):
    with (c / 'traces' / (arm + '.bve-candidates.tsv')).open() as stream:
        rows = h.candidates(stream, 'tactix' if arm == 'candidate' else 'kissat', 387253)
    groups[arm] = h.attempts(rows)
    skipped = [a for a in groups[arm] if 'postflush' not in a]
    for a in skipped:
        assert arm == 'candidate' and a['end']['applied'] == 0
        for key in ('roots', 'trail', 'propagate', 'value'):
            assert a['begin'][key] == a['end'][key], (key, a)
    omitted[arm] = len(skipped)
def portable(a):
    return {stage: {key: a[stage][key] for key in ('external', 'conflicts', 'applied', 'value')}
            for stage in ('begin', 'end')}
first = None
for i, (a,b) in enumerate(itertools.zip_longest(groups['candidate'], groups['kissat'])):
    if a is None or b is None or portable(a) != portable(b):
        first = dict(attempt=i, candidate=a, kissat=b); break
assert first
extra = set(map(int, json.loads((c / 'schedule-summary-7020.json').read_text())['candidate_only']))
extra_attempts = [dict(attempt=i, candidate=a) for i,a in enumerate(groups['candidate']) if a['begin']['external'] in extra]
assert extra_attempts
effort = []
for i, (a,b) in enumerate(zip(groups['candidate'], groups['kissat'])):
    if portable(a) != portable(b): break
    if any(a[stage][key] != b[stage][key] for stage in ('begin', 'end') for key in ('ticks', 'limit')):
        effort.append(dict(attempt=i, candidate=a, kissat=b))
out = dict(hashes=hashes, first_attempt_difference=first, first_extra_member_attempt=extra_attempts[0],
    omitted_postflush_rows=omitted, extra_member_attempts=extra_attempts, common_prefix_effort_mismatches=effort,
    limitation='7021 raw row mismatch at74 is an optional postflush row after a root-neutral declined attempt, not the first candidate/order effect. All omissions retained and checked against own unchanged root/trail/propagation/value state. Compare complete begin/end groups, not shifted raw rows. A schedule discrepancy remains a causal lead, not a default repair verdict.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({key: out[key] for key in ('first_attempt_difference', 'first_extra_member_attempt', 'omitted_postflush_rows')}, indent=2))
print('common effort mismatches', len(effort))
