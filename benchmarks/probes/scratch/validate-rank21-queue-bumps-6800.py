#!/usr/bin/env python3
"""Preserve exact complete6794 replay before interpreting pre-move queue rows."""
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from queue_bump_trace import read_bumps
c = root/'benchmarks/runs/20260909-6798-rank21-queue-bumps'
target = c/'summary-6800.json'
assert not target.exists()
if not all((c/f'results/{arm}.json').exists() for arm in ('candidate','kissat')):
    print('Queue-bump outcomes pending; no summary written'); raise SystemExit(0)
m = json.loads((c/'manifest.json').read_text())
hashes = {}; bumps = {}; replay = {}
def bind(p,expected=None):
    digest = sha(p); assert expected is None or digest == expected,p
    hashes[str(p.relative_to(root))] = digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
for arm in ('candidate','kissat'):
    p=c/f'results/{arm}.json'; bind(p); r=json.loads(p.read_text())
    old_path=root/m['cached_controls'][arm]/f'results/{arm}.json'; bind(old_path)
    old=json.loads(old_path.read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==arm
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
    assert r['partial_certificate']==old['partial_certificate']
    if arm=='kissat': assert r['prior_certificate_prefix']==old['prior_certificate_prefix']
    assert set(r['trace_sha256']) == set(old['trace_sha256']) | {arm+'.bumps.tsv'}
    assert all(r['trace_sha256'][rel]==digest for rel,digest in old['trace_sha256'].items())
    for rel,digest in r['trace_sha256'].items(): bind(c/'traces'/rel,digest)
    replay[arm]=dict(all_original_traces_exact=True,partial_proof_exact=True,scratch_deleted=True)
    bumps[arm]=read_bumps((c/f'traces/{arm}.bumps.tsv').read_text(),window=tuple(m['bump_window']),cap=m['bump_row_cap'])
def first_difference(fields):
    key=lambda row:tuple(row[k] for k in fields) if row is not None else None
    return next((dict(index=i,candidate=a,kissat=b) for i,(a,b) in enumerate(itertools.zip_longest(bumps['candidate'],bumps['kissat'])) if key(a)!=key(b)),None)
for p in (Path(__file__),root/'tools/bench/queue_bump_trace.py',root/'tools/bench/test_queue_bump_trace.py'): bind(p)
out=dict(hashes=hashes,observer_replay_validated=True,replay=replay,counts={a:len(rr) for a,rr in bumps.items()},
    first_move_identity_difference=first_difference(('conflicts','decisions','stable','variable')),
    first_links_values_difference=first_difference(('conflicts','decisions','stable','variable','previous','next','value')),
    selected_moves={a:[r for r in rr if r['variable']==m['observed_variable']] for a,rr in bumps.items()},
    all_bump_rows=bumps,
    limitation='Pre-move rows retain duplicate requests, absolute clocks and links. First row mismatch is localization, not proof that a queue implementation is wrong; inspect analysis admission/order and earlier state. Partial proof is not final acceptance.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','all_bump_rows')},indent=2))
