#!/usr/bin/env python3
"""Bind portable forward/BVE boundaries around the first Rank32 decision split."""
import csv
import hashlib
import itertools
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'
target = c / 'boundary-localization.json'
assert not target.exists()
summary = json.loads((c / 'summary.json').read_text())
conflict = int(summary['first_decision_difference']['candidate'][0])
fields = ['name', 'stage', 'conflicts', 'decisions', 'active', 'unassigned', 'irred', 'red', 'binary']
all_rows = {}
hashes = {}
for arm in ('candidate', 'kissat'):
    p = c / 'traces' / (arm + '.boundaries.tsv')
    digest = hashlib.sha256(p.read_bytes()).hexdigest()
    result = json.loads((c / 'results' / (arm + '.json')).read_text())
    assert result['exit_code'] == 0 and result['scratch_deleted']
    assert digest == result['trace_sha256'][p.name]
    hashes[str(p.relative_to(root))] = digest
    lines = p.read_text().splitlines()
    reader = csv.DictReader(lines[2:], fieldnames=lines[1][2:].split('\t'), delimiter='\t')
    selected = []
    for row in reader:
        if row['name'].lower() not in ('strengthen', 'bve') or int(row['conflicts']) > conflict:
            continue
        portable = {f: row[f].lower() if f == 'name' else row[f] for f in fields}
        selected.append(portable)
    all_rows[arm] = selected
pairs = list(itertools.zip_longest(all_rows['candidate'], all_rows['kissat']))
differences = [dict(ordinal=i, candidate=a, kissat=b) for i, (a, b) in enumerate(pairs) if a != b]
selected = {arm: [r for r in rows if int(r['conflicts']) == conflict] for arm, rows in all_rows.items()}
assert selected['candidate'][0] == selected['kissat'][0]
assert selected['candidate'][1]['name'] == selected['kissat'][1]['name'] == 'strengthen'
assert selected['candidate'][1]['stage'] == selected['kissat'][1]['stage'] == 'end'
assert int(selected['candidate'][1]['irred']) - int(selected['kissat'][1]['irred']) == -8
out = dict(conflict=conflict, portable_fields=fields, hashes=hashes,
           compared_boundaries=len(pairs), unequal_boundaries=len(differences),
           first_portable_difference=differences[0] if differences else None,
           selected_boundary_pairs=selected,
           conclusion='At the first decision split, Strengthen enters with equal portable counts and exits with eight fewer Tactix irredundant large clauses; BVE then changes counts. Unit-exit cause is not established for Rank32.',
           limitation='Counts are not a physical formula/flag identity proof. Trail and propagation totals are representation-local and deliberately not compared.',
           tests='Header-aware prefix validator has four tests. This extraction only selects named recorded columns; asserts entry equality and measured eight-clause delta without changing solver, oracle or source.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))
