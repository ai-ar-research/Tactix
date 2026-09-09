#!/usr/bin/env python3
"""Full fixed-50 view and parent work comparison; no cross-window causal claim."""
import json
import math
from pathlib import Path
import re
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c = root / 'benchmarks/runs/20260909-6239-substitute-retry-ranks1-50'
p = root / 'benchmarks/runs/20260909-6181-learned-sort-ranks1-50'
checkpoint = root / 'benchmarks/reports/2026-09-09-substitute-retry-checkpoint-6255.json'
s = json.loads(checkpoint.read_text())['campaigns'][c.name]['score']
assert s['records'] == s['expected_solves'] == 50
out = root / 'benchmarks/reports/2026-09-09-retry-full50-6258'
assert not out.exists()
rows = []; hashes = {str(checkpoint.relative_to(root)): sha(checkpoint)}
def read(c, rank):
    path = c / 'results' / f'{rank-1:03}.json'
    if not path.exists(): path = c / 'provisional' / path.name
    r = json.loads(path.read_text())
    state = classify(r, path.parent.name == 'results')
    assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT')
    hashes[str(path.relative_to(root))] = sha(path)
    return r, state
def conflicts(r):
    matches = [re.fullmatch(r'c conflicts\s+(\d+)', line) for line in r['solver_statistics']]
    values = [int(m[1]) for m in matches if m]
    assert len(values) == 1
    return values[0]
for row in s['per_rank']:
    rank = row['rank']; a, state = read(c, rank); b, _ = read(p, rank)
    assert a['instance'] == b['instance'] and a['expected'] == b['expected']
    assert row['candidate_par2_seconds'] == a['solve']['wall_seconds']
    rows.append(dict(row, instance=a['instance'], expected=a['expected'], validation=state['status'],
        parent_seconds=b['solve']['wall_seconds'], candidate_conflicts=conflicts(a), parent_conflicts=conflicts(b),
        certificate_exact_parent=a['certificate_sha256'] == b['certificate_sha256'],
        candidate_node=a['node'], parent_node=b['node'],
        gap_seconds=row['candidate_par2_seconds'] - row['kissat_seconds']))
groups = {}
for name, selected in (('original40', rows[:40]), ('added10', rows[40:])):
    a = math.fsum(r['candidate_par2_seconds'] for r in selected)
    b = math.fsum(r['kissat_seconds'] for r in selected)
    groups[name] = dict(candidate_seconds=a, kissat_seconds=b, ratio=a/b, gap_seconds=a-b)
result = dict(candidate_seconds=s['candidate_par2'], kissat_seconds=s['fixed_kissat_seconds'], ratio=s['ratio'],
    solver_outcomes=50, checked=sum(r['validation'].startswith('checked_') for r in rows),
    groups=groups, changed_certificate_ranks=[r['rank'] for r in rows if not r['certificate_exact_parent']],
    rows=rows, hashes=hashes,
    limitation='Cross-window provisional aggregate; not a causal repair speedup/regression estimate. No rejection observed; pending checks block acceptance, not development. Three paired timing waves, source fuzz and memory gate remain open.')
out.mkdir()
(out / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
fields = list(rows[0])
(out / 'comparison.tsv').write_text('\t'.join(fields) + '\n' + ''.join(
    '\t'.join(str(r[k]) for k in fields) + '\n' for r in rows))
print(json.dumps({k:v for k,v in result.items() if k not in ('rows','hashes')}, indent=2))
