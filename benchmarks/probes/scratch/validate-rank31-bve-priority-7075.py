#!/usr/bin/env python3
"""Require full replay before interpreting four selected heap histories."""
import importlib.util
import io
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7073-rank31-bve-priority'
target = c / 'solver-summary-7075.json'
hashes = {}
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
def module(name, filename):
    p = Path(__file__).with_name(filename); bind(p)
    spec = importlib.util.spec_from_file_location(name, p)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result
h = module('own_repeat', 'submit-collect-retry-wave-7042.py')
b = module('heap_reader', 'validate-rank32-selected-heaps-6196.py')
h.fixtures(); b.fixtures()
b.SUBJECTS = (355342, 355344, 497326, 497345)
header = '# bve-heap-observer\t1\n'
rows = [f'tactix\tstage=initial\ttrigger=0\tsubject={s}\tscore=-0x1p+0\tpositive=1\tnegative=2\tpos={i}\troot=355342\tsize=4\n'
        for i, s in enumerate(b.SUBJECTS)]
assert len(b.parse(io.StringIO(header + ''.join(rows + rows)), 'tactix')) == 2
for malformed in (rows[:3], [rows[1], rows[0], *rows[2:]]):
    try: b.parse(io.StringIO(header + ''.join(malformed)), 'tactix')
    except AssertionError: pass
    else: raise AssertionError('invalid four-subject framing accepted')
print('PASS: selected four-subject/two-round framing; incomplete and reordered initial rows rejected')
if '--self-test' in sys.argv: sys.exit(0)
assert not target.exists(), 'successful output is frozen'
paths = [c / f'results/{task:03d}.json' if (c / f'results/{task:03d}.json').exists()
         else c / f'provisional/{task:03d}.json' for task in range(2)]
if not all(p.exists() for p in paths):
    print('Priority solver outcomes pending; no output written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text()); parent = root / m['parent']
assert tuple(m['subjects']) == b.SUBJECTS
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', Path(__file__)): bind(p)
records = {}; observed = {}
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
    for suffix in ('macro.tsv', 'boundaries.tsv', 'bve-candidates.tsv'):
        p = c / 'traces' / (arm + '.' + suffix); old = parent / 'traces' / p.name
        bind(old); bind(p, sha(old))
    p = c / 'traces' / (arm + '.bve-heap.tsv'); bind(p)
    with p.open() as stream: rounds = b.parse(stream, 'tactix')
    records[arm] = dict(path=str(paths[task].relative_to(root)), state=state,
                        full_release_replay=True, whole_parent_trace_replay=True)
    observed[arm] = dict(rounds=len(rounds), rows=[len(v) for v in rounds],
                         initial=[v[:len(b.SUBJECTS)] for v in rounds], history=rounds)
out = dict(hashes=hashes, records=records, subjects=b.SUBJECTS, observed=observed,
    limitation='Exact full Release work/certificate and all7068 macro/boundary/attempt bytes precede selected heap interpretation. These four subjects do not prove complete schedule/formula identity or establish the earliest native divergence. Proof acceptance and cleanup remain pending until published checked/clean records.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps({arm: {k: v for k, v in values.items() if k != 'history'} for arm, values in observed.items()}, indent=2))
