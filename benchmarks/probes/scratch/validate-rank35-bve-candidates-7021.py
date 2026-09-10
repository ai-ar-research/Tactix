#!/usr/bin/env python3
"""Require full own replay, then compare selected BVE lifecycle and effort."""
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
h.fixtures()
c = root / 'benchmarks/runs/20260910-7018-rank35-bve-frontier'
target = c / 'candidate-summary-7021.json'
assert not target.exists()
if any(not (c / 'results' / (a + '.json')).exists() for a in ('candidate', 'kissat')):
    print('Candidate tasks pending; no summary written'); sys.exit(0)
m = json.loads((c / 'manifest.json').read_text()); parent = root / m['parent']
hashes = {}
def bind(p, expected=None):
    actual = sha(p)
    assert expected is None or actual == expected, p
    hashes[str(p.relative_to(root))] = actual
for rel, digest in m['hashes'].items(): bind(c / rel, digest)
for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
for p in (c / 'manifest.json', c / 'launch.json', Path(__file__), reader): bind(p)
rows, groups = {}, {}
for arm in ('candidate', 'kissat'):
    p = c / 'results' / (arm + '.json'); bind(p)
    r = json.loads(p.read_text())
    old = json.loads((parent / 'results' / p.name).read_text())
    assert r['arm'] == arm and r['exit_code'] == 0 and r['scratch_deleted']
    assert not Path(r['scratch']).exists()
    assert r['binary_sha256'] == old['binary_sha256'] == m['hashes']['pins/' + arm]
    assert r['partial_certificate'] == old['partial_certificate']
    assert set(r['trace_sha256']) == set(old['trace_sha256']) | {arm + '.bve-candidates.tsv'}
    for name, digest in old['trace_sha256'].items():
        assert r['trace_sha256'][name] == digest
        bind(parent / 'traces' / name, digest)
    for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
    path = c / 'traces' / (arm + '.bve-candidates.tsv')
    assert path.read_bytes().endswith(b'\n'), 'truncated candidate row'
    with path.open() as stream:
        rows[arm] = h.candidates(stream, 'tactix' if arm == 'candidate' else 'kissat', m['selected_conflict'])
    groups[arm] = h.attempts(rows[arm])
fields = ('external', 'stage', 'conflicts', 'applied', 'value')
first = next((dict(index=i, candidate=a, kissat=b,
                  context={arm: rr[max(0,i-3):i+4] for arm, rr in rows.items()})
              for i, (a,b) in enumerate(itertools.zip_longest(rows['candidate'], rows['kissat']))
              if a is None or b is None or any(a[key] != b[key] for key in fields)), None)
effort = None
for i, (a,b) in enumerate(zip(groups['candidate'], groups['kissat'])):
    if a['begin']['external'] != b['begin']['external']: break
    da, db = a['end']['ticks']-a['begin']['ticks'], b['end']['ticks']-b['begin']['ticks']
    assert da >= 0 and db >= 0
    if da != db:
        effort = dict(attempt=i, candidate_delta=da, kissat_delta=db, candidate=a, kissat=b); break
out = dict(hashes=hashes, own_full1m_trace_and_partial_proof_replay=True,
    compared_fields=fields, rows={arm: len(rr) for arm, rr in rows.items()},
    attempts={arm: len(gg) for arm, gg in groups.items()}, first_portable_difference=first,
    first_effort_delta_difference=effort,
    limitation='Complete own trace/proof replay establishes observer neutrality. Selected first order/effect/effort difference is a causal lead, not proof of worse utility or an accepted repair. Raw flags/trail/root/ticks retained with their local meanings.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(json.dumps({key: out[key] for key in ('rows', 'attempts', 'first_portable_difference', 'first_effort_delta_difference')}, indent=2))
