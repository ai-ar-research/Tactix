#!/usr/bin/env python3
"""Bind full-release replay and common2M prefix before localizing later boundaries."""
import importlib.util
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from search_window import macro_replay
c = root / 'benchmarks/runs/20260910-7064-rank31-full-boundaries'
prefix = root / 'benchmarks/runs/20260910-7055-collect-retry-rank31-2m'
target = c / 'solver-summary-7066.json'
assert not target.exists()
paths = [c / f'results/{task:03d}.json' if (c / f'results/{task:03d}.json').exists()
         else c / f'provisional/{task:03d}.json' for task in range(2)]
if not all(p.exists() for p in paths):
    print('Full boundary solver outcomes pending; no output written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text()); hashes = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or expected == digest, p
    hashes[str(p.relative_to(root))] = digest
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', Path(__file__), root / 'tools/bench/search_window.py'): bind(p)
def module(name, filename):
    p = Path(__file__).with_name(filename); bind(p)
    spec = importlib.util.spec_from_file_location(name,p)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result
h = module('own_repeat', 'submit-collect-retry-wave-7042.py'); h.fixtures()
b = module('boundaries', 'localize-root-lock-rank37-6073.py'); b.fixtures()
records = {}; raw = {}; projected = {}; replay = {}
for task, arm in enumerate(('candidate','previous')):
    p = paths[task]; bind(p); r = json.loads(p.read_text())
    state = classify(r,p.parent.name=='results')
    assert state['status'] in ('pending_validation','checked_UNSAT'), (p,state)
    assert r['task'] == task and r['rank'] == 31 and r['arm'] == arm
    if p.parent.name == 'results': assert r['binary_sha256'] == m['arms'][arm]['sha256']
    oracle = m['unprofiled_records']['31/' + arm]; op = root / oracle['path']
    bind(op, oracle['sha256'])
    assert h.fingerprint(r) == h.fingerprint(json.loads(op.read_text())), (arm,'full Release work/proof drift')
    assert r['solve']['solver_environment'] == m['arms'][arm]['environment']
    records[arm] = dict(record=str(p.relative_to(root)), state=state, full_release_replay=True)
    raw[arm] = {}; replay[arm] = {}
    for suffix in ('macro.tsv','boundaries.tsv'):
        p = c / 'traces' / (arm + '.' + suffix); bind(p)
        body = p.read_text(); assert body.endswith('\n')
        oldp = prefix / 'traces' / (arm + '.' + suffix); bind(oldp)
        old = oldp.read_text(); assert body.startswith(old), (arm,suffix,'cached2M prefix changed')
        raw[arm][suffix] = body.splitlines()
        replay[arm][suffix] = dict(cached_rows=len(old.splitlines())-2, total_rows=len(body.splitlines())-2)
        if suffix == 'macro.tsv': macro_replay(body,body)
        else:
            with p.open() as stream: projected[arm], ignored = b.parse(stream,False)
def first(left,right):
    for i,(a,z) in enumerate(itertools.zip_longest(left,right)):
        if a != z:
            return dict(line=i+1,candidate=a,previous=z,
                        preceding_candidate=left[max(0,i-6):i],preceding_previous=right[max(0,i-6):i])
    return None
out = dict(hashes=hashes,records=records,cached2m_replay=replay,
    raw_first_differences={suffix:first(raw['candidate'][suffix],raw['previous'][suffix]) for suffix in ('macro.tsv','boundaries.tsv')},
    projected_boundary_difference=b.compare(projected['candidate'],projected['previous']),
    limitation='Exact full own Release work/proof replay and cached2M raw-prefix replay precede localization. These are coarse phase/CDCL macro differences, not the first decision split or a proof of formula strength. Independent certificates remain pending until published checked/clean records.')
with target.open('x') as stream: json.dump(out,stream,indent=2);stream.write('\n')
print(target)
print(json.dumps(out['raw_first_differences'],indent=2))
