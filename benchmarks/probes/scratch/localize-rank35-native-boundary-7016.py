#!/usr/bin/env python3
"""Project bound rule boundaries without equating local counters or formulas."""
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7011-rank35-current-native-prefix'
target = c / 'boundary-localization-7016.json'
assert not target.exists()
p = c / 'summary-7015.json'
s = json.loads(p.read_text())
assert s['matching_rows_from_start'] == 79224352
assert all(s['inherited100k'].values())
names = dict(congruence='Congruence', substitute_early='Substitute',
             substitute_late='Substitute', backbone_early='Backbone',
             backbone_late='Backbone', vivify='Vivify', sweep='Sweep',
             transitive='TransitiveReduce', factor='Factor', strengthen='Strengthen', bve='BVE')
fields = 'solver seq name stage conflicts decisions propagations active unassigned trail irred red binary'.split()
hashes = {str(p.relative_to(root)): sha(p), str(Path(__file__).relative_to(root)): sha(Path(__file__))}
rows, projected = {}, {}
for arm in ('candidate', 'kissat'):
    p = c / 'traces' / (arm + '.boundaries.tsv')
    digest = sha(p)
    assert digest == s['hashes'][str(p.relative_to(root))]
    hashes[str(p.relative_to(root))] = digest
    content = p.read_text()
    lines = content.splitlines()
    solver = 'tactix' if arm == 'candidate' else 'kissat'
    header = fields + (['ticks', 'search_ticks'] if arm == 'candidate' else [])
    assert content.endswith('\n')
    assert lines[:2] == ['# ' + solver + '-inprocess-boundary\t1', '# ' + '\t'.join(header)]
    rr, pp = [], []
    for sequence, line in enumerate(lines[2:]):
        values = line.split('\t')
        assert len(values) == len(header)
        row = dict(zip(header, values))
        assert row['solver'] == solver and row['seq'] == str(sequence)
        assert row['stage'] in ('begin', 'end')
        for key in header:
            if key not in ('solver', 'name', 'stage'):
                assert row[key].isdigit()
                row[key] = int(row[key])
        rr.append(row)
        if row['conflicts'] == 0 or row['name'] not in (names if arm == 'kissat' else names.values()):
            continue
        pp.append(dict(name=names.get(row['name'], row['name']), **{key: row[key] for key in
                       ('stage', 'conflicts', 'decisions', 'active', 'irred', 'red', 'binary')}))
    rows[arm], projected[arm] = rr, pp
first = next((dict(index=i, candidate=a, kissat=b) for i, (a, b) in enumerate(
    itertools.zip_longest(projected['candidate'], projected['kissat'])) if a != b), None)
assert first and first['candidate']['conflicts'] == first['kissat']['conflicts'] == 387253
assert first['candidate']['name'] == first['kissat']['name'] == 'BVE'
assert first['candidate']['stage'] == first['kissat']['stage'] == 'end'
out = dict(hashes=hashes, first_projected_nonzero_difference=first,
    selected_raw_boundaries={arm: [r for r in rr if r['conflicts'] == 387253] for arm, rr in rows.items()},
    prior_probe_raw_boundaries={arm: [r for r in rr if r['conflicts'] == 372993] for arm, rr in rows.items()},
    decision='Compare selected first BVE physical schedule and all candidate order/effects/budget at387253. The extra eliminations are not inherently harmful; establish the cause and its downstream consequence before repair.',
    limitation='Projected counts only, excluding startup and local propagation/trail/tick currencies. Matching counts/decisions do not prove formula identity.7013 missing-native-macro failure is separately classified by7015; native inprocessing boundaries suffice here.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps(first, indent=2))
