#!/usr/bin/env python3
"""Separate root-false storage differences from Rank31's effective formula."""
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5815-rank31-forward-residual-formula'
out = c / 'root-normalized-followup.json'
assert not out.exists()
spec = importlib.util.spec_from_file_location('formula', Path(__file__).with_name('compare-rank32-formula-5785.py'))
parser = importlib.util.module_from_spec(spec)
spec.loader.exec_module(parser)
parser.fixtures()

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

def normalize(row):
    if any(value > 0 for _, value in row[3]):
        return None
    return row[0][0], tuple(lit for lit, value in row[3] if value == 0)

assert normalize(((0, (-3, 1, 2)), (), (), ((-3, -1), (1, 0), (2, 0)))) == (0, (1, 2))
assert normalize(((0, (-3, 1, 2)), (), (), ((-3, 1), (1, 0), (2, 0)))) is None
summary = json.loads((c / 'summary.json').read_text())
assert summary['observer_parent_trace_and_proof_equal']
sets, counts, bindings = {}, {}, {}
for arm in ('candidate', 'kissat'):
    path = c / 'traces' / (arm + '.formula.tsv')
    digest = sha(path)
    assert digest == summary['evidence'][arm]['trace_sha256'][path.name]
    effective = Counter()
    satisfied = 0
    for row in parser.read_rows(path):
        key = normalize(row)
        if key is None:
            satisfied += 1
        else:
            effective[key] += 1
    sets[arm] = effective
    counts[arm] = dict(effective_copies=sum(effective.values()), root_satisfied_copies=satisfied)
    bindings[str(path.relative_to(root))] = digest
extras = {arm: [dict(redundant=key[0], literals=key[1], copies=n) for key, n in
                (sets[arm] - sets[other]).items()]
          for arm, other in [('candidate', 'kissat'), ('kissat', 'candidate')]}
native = root / 'tools/solvers/variants/kissat-score-bump-observer-5696/src'
source = root / '.claude/worktrees/runtime-forward-unit-5798/src'
paths = [native / 'eliminate.c', native / 'forward.c', native / 'eventtrace.c',
         source / 'policies/default_policy.cpp', source / 'rules/bve/bve.cpp',
         source / 'kernel/kernel.cpp', source / 'rules/propagate/propagate.hpp']
bindings.update({str(p.relative_to(root)): sha(p) for p in paths})
result = dict(effective_counts=counts, effective_extras=extras,
              effective_equal=sets['candidate'] == sets['kissat'], hashes=bindings,
              boundary_correction='Native formula hook is inside forward.c at set_boundary(Strengthen,end),before eliminate.c reconnects all irredundant large clauses,resets propagate,and calls flush_units_while_connected. Portable strengthen-end row is after that caller step. Physical493638 versus recorded493635 is not evidence of an incorrect native counter.',
              next_action='Test the missing full-connection propagation/cleanup before BVE scheduling. Tactix run_batch_entry uses sparse propagation after partial forward propagation,while BVE cleanup helper alone does not propagate. Preserve sparse phase-exit correctness;do not substitute cleanup-only for full closure.',
              limitation='Root-normalization is explanatory and emits no solver/proof mutation. No incoming full formula or native post-caller formula snapshot was captured;these are different observation boundaries.')
out.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({k: v for k, v in result.items() if k != 'hashes'}, indent=2))
