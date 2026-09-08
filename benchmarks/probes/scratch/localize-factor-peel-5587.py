#!/usr/bin/env python3
"""Reconstruct the candidate peel from validated physical input, not runtime timing."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import runpy

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5585-rank32-factor-literal'
inputs = root / 'benchmarks/runs/20260908-5569-rank32-second-factor-input'
support = runpy.run_path(str(Path(__file__).with_name('compare-small-factor-5566.py')))
sha = support['sha']
prior = json.loads((c / 'literal-localization.json').read_text())
assert prior['parent_replay']
target = tuple(prior['pass_comparisons'][1]['candidate_only'][0]['entry'][2])
assert target == (-491572, -41179, 41096, 119873, 119877)
evidence, arms = {}, {}
for arm in ('candidate', 'kissat'):
    result = json.loads((inputs / 'results' / (arm + '.json')).read_text())
    for name, digest in result['trace_sha256'].items():
        if name.endswith(('.formula.tsv', '.binary.tsv')):
            assert sha(inputs / 'traces' / name) == digest
            evidence[name] = digest
    raw = (inputs / 'traces' / (arm + '.formula.tsv')).read_text().splitlines()
    formulas = []
    for line in raw:
        if line.startswith('#'): continue
        fields = line.split('\t')
        lits = tuple(map(int, fields[3].split(',')))
        assert len(lits) == int(fields[1]) and lits == tuple(sorted(lits))
        if fields[0] == '0' and len(lits) <= 5: formulas.append(lits)
    binaries = support['binary_snapshots']((inputs / 'traces' / (arm + '.binary.tsv')).read_text(),
                                         'tactix' if arm == 'candidate' else arm)[1]
    arms[arm] = (formulas, binaries)
assert arms['candidate'] == arms['kissat']
formulas, binaries = arms['candidate']
bincount = Counter(l for pair in binaries for l in pair)
def counts(clauses): return Counter(l for clause in clauses for l in clause)
def peel(clauses, degree): return [cl for cl in clauses if all(bincount[l] + degree[l] >= 2 for l in cl)]
degree = counts(formulas)
rounds = []
for n in range(3):
    rounds.append(dict(round=n, candidates=len(formulas), target_present=target in formulas,
        target_degrees=[dict(lit=l, binaries=bincount[l], large=degree[l]) for l in target]))
    reduced = peel(formulas, degree)
    if n < 2: formulas, degree = reduced, counts(reduced)
connected = peel(formulas, degree)
result = dict(input_identity=True, evidence=evidence,
    selected_authority_sha256=sha(c / 'literal-localization.json'), target=target,
    rounds=rounds, target_connected=target in connected,
    target_binary_partners={str(l):[p[1] if p[0] == l else p[0] for p in binaries if l in p] for l in target},
    target_large_neighborhood={str(l):[cl for cl in connected if l in cl] for l in target if degree[l] < 4},
    limitation='Read-only mathematical reconstruction from shared snapshots; not an actual-code parity test. Native bincount skips inactive smaller endpoints, while this census includes every physical binary. Active flags, transient dense-entry state, and native last-irredundant frontier remain unobserved.')
p = c / 'peel-localization.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
