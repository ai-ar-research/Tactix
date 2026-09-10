#!/usr/bin/env python3
"""Validate full observer neutrality before comparing selected BVE attempts."""
import importlib.util
import itertools
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha

c = root / 'benchmarks/runs/20260910-7068-rank31-selected-bve'
target = c / 'solver-summary-7070.json'
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
def module(name, filename):
    p = Path(__file__).with_name(filename); bind(p)
    spec = importlib.util.spec_from_file_location(name, p)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result
h = module('own_repeat', 'submit-collect-retry-wave-7042.py')
b = module('bve_reader', 'validate-rank32-bve-candidates-6192.py')
h.fixtures(); b.fixtures()
if '--self-test' in sys.argv: sys.exit(0)
assert not target.exists(), 'successful output is frozen; do not rerun'
paths = [c / f'results/{task:03d}.json' if (c / f'results/{task:03d}.json').exists()
         else c / f'provisional/{task:03d}.json' for task in range(2)]
if not all(p.exists() for p in paths):
    print('Selected BVE solver outcomes pending; no output written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text())
parent = root / m['parent']
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', Path(__file__)): bind(p)
records = {}; groups = {}; counts = {}
for task, arm in enumerate(('candidate', 'previous')):
    p = paths[task]; bind(p); r = json.loads(p.read_text())
    state = classify(r, p.parent.name == 'results')
    assert state['status'] in ('pending_validation', 'checked_UNSAT'), (p, state)
    assert r['task'] == task and r['rank'] == 31 and r['arm'] == arm
    if p.parent.name == 'results': assert r['binary_sha256'] == m['arms'][arm]['sha256']
    oracle = m['unprofiled_records']['31/' + arm]; op = root / oracle['path']
    bind(op, oracle['sha256'])
    assert h.fingerprint(r) == h.fingerprint(json.loads(op.read_text())), (arm, 'full Release drift')
    assert r['solve']['solver_environment'] == m['arms'][arm]['environment']
    for suffix in ('macro.tsv', 'boundaries.tsv'):
        p = c / 'traces' / (arm + '.' + suffix)
        old = parent / 'traces' / p.name
        bind(old); bind(p, sha(old))
    p = c / 'traces' / (arm + '.bve-candidates.tsv'); bind(p)
    with p.open() as stream: rows = b.candidates(stream, 'tactix', m['selected_conflict'])
    for row in rows:
        assert {'retry', 'retired', 'roots', 'trail', 'propagate'} <= row.keys()
        assert row['retry'] in (0, 1) and row['retired'] in (0, 1)
    groups[arm] = b.attempts(rows)
    omitted = 0
    for a in groups[arm]:
        assert a['end']['ticks'] >= a['begin']['ticks']
        assert len({row['limit'] for row in a.values()}) == 1
        if 'postflush' not in a:
            omitted += 1
            assert a['end']['applied'] == 0
            for key in ('roots', 'trail', 'propagate', 'value'):
                assert a['begin'][key] == a['end'][key], (arm, key, a)
    records[arm] = dict(path=str(paths[task].relative_to(root)), state=state,
                        full_release_replay=True, whole_boundary_replay=True)
    counts[arm] = dict(rows=len(rows), attempts=len(groups[arm]), omitted_noop_postflush=omitted)

def first_difference(project):
    for i, (a, z) in enumerate(itertools.zip_longest(groups['candidate'], groups['previous'])):
        if a is None or z is None or project(a) != project(z):
            return dict(attempt=i, candidate=a, previous=z,
                        preceding_candidate=groups['candidate'][max(0, i-3):i],
                        preceding_previous=groups['previous'][max(0, i-3):i])
    return None
identity_outcome = lambda a: {stage: {key: a[stage][key] for key in ('external', 'applied', 'value', 'retired')}
                              for stage in ('begin', 'end')}
differences = dict(all_fields=first_difference(lambda a: a),
                   identity_outcome=first_difference(identity_outcome))
out = dict(hashes=hashes, records=records, counts=counts, first_differences=differences,
    limitation='Complete own Release work/certificate and full7064 boundary replay establish diagnostic neutrality. Attempts at the first differing BVE phase localize changed eligibility/order/effort/outcome; they do not prove the first native decision split, root cause or formula strength. Independent proof checking remains pending unless published checked/clean.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps(dict(counts=counts, first_differences=differences), indent=2))
