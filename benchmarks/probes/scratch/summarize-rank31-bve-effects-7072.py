#!/usr/bin/env python3
"""Separate initial order changes from the actual lost elimination."""
from collections import Counter, defaultdict
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7068-rank31-selected-bve'
target = c / 'effect-summary-7072.json'
assert not target.exists()
s = json.loads((c / 'solver-summary-7070.json').read_text())
assert all(r['full_release_replay'] and r['whole_boundary_replay'] for r in s['records'].values())
hashes = dict(s['hashes'])
for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
reader = Path(__file__).with_name('validate-rank32-bve-candidates-6192.py')
spec = importlib.util.spec_from_file_location('bve_reader', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
for p in (Path(__file__), reader, c / 'solver-summary-7070.json'):
    hashes[str(p.relative_to(root))] = sha(p)
groups = {}; by = {}; counts = {}; eliminated = {}; attempts = {}
for arm in ('candidate', 'previous'):
    with (c / 'traces' / (arm + '.bve-candidates.tsv')).open() as stream:
        groups[arm] = h.attempts(h.candidates(stream, 'tactix', 5187498))
    by[arm] = defaultdict(list)
    for i, a in enumerate(groups[arm]): by[arm][a['begin']['external']].append(dict(attempt=i, record=a))
    eliminated[arm] = {a['begin']['external'] for a in groups[arm] if a['end']['applied']}
    attempts[arm] = Counter(a['begin']['external'] for a in groups[arm])
    limits = {row['limit'] for a in groups[arm] for row in a.values()}
    assert limits == {192068775}
    counts[arm] = dict(attempts=len(groups[arm]), eliminated=len(eliminated[arm]),
                       entry_ticks=groups[arm][0]['begin']['ticks'],
                       end_ticks=groups[arm][-1]['end']['ticks'], limit=limits.pop())
    assert counts[arm]['end_ticks'] < counts[arm]['limit']
changed = eliminated['candidate'] ^ eliminated['previous']
assert changed == {355342}
out = dict(hashes=hashes, counts=counts,
    candidate_only_eliminations=sorted(eliminated['candidate'] - eliminated['previous']),
    previous_only_eliminations=sorted(eliminated['previous'] - eliminated['candidate']),
    different_elimination_attempts={arm: {str(v): by[arm][v] for v in sorted(changed)} for arm in by},
    extra_attempt_counts={arm: dict(attempts[arm] - attempts[other]) for arm, other in
                           (('candidate', 'previous'), ('previous', 'candidate'))},
    initial_swap={arm: groups[arm][11:13] for arm in groups},
    next_subjects=[355342, 355344, 497326, 497345],
    decision='First11 attempts exact, then497345/497326 swap; each processes the other next with the same individual outcome and combined66 ticks. The sole differing eliminated variable355342 is tried at candidate14433/previous6347 (zero-based), declines/applies. Inspect its incoming score/heap position and producer-triggered updates, plus355344 and the first swapped pair. No budget increase: both finish far below the identical ceiling.',
    limitation='Per-variable matching outcomes do not prove commutativity or clause-pose equality. The unique elimination difference is a witness, not proof that one variable causes the full runtime regression or that either formula is stronger.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps({k: v for k, v in out.items() if k not in ('hashes', 'initial_swap', 'different_elimination_attempts')}, indent=2))
