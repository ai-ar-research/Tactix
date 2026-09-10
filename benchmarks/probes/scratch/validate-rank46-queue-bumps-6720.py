#!/usr/bin/env python3
"""Require complete own replay before comparing the focused queue moves."""
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from queue_bump_trace import read_bumps
c=root/'benchmarks/runs/20260909-6718-rank46-queue-bumps'
target=c/'summary-6720.json'; assert not target.exists()
if any(not (c/f'results/{a}.json').exists() for a in ('candidate','kissat')):
    print('Pending queue diagnostic; no summary'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text()); hashes={}; bumps={}; choices={}; replay={}
def bind(p,expected=None):
    digest=sha(p); assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
for arm in ('candidate','kissat'):
    p=c/f'results/{arm}.json'; bind(p); r=json.loads(p.read_text())
    parent=root/m['cached_controls'][arm]; old=json.loads((parent/f'results/{arm}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==arm
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
    for key in ('partial_certificate','prior_certificate_prefix'): assert r[key]==old[key]
    assert set(r['trace_sha256'])-set(old['trace_sha256'])=={arm+'.bumps.tsv',arm+'.choice.tsv'}
    assert all(r['trace_sha256'].get(name)==digest for name,digest in old['trace_sha256'].items())
    for name,digest in r['trace_sha256'].items(): bind(c/'traces'/name,digest)
    replay[arm]=dict(all_original_traces_exact=True,original_trace_count=len(old['trace_sha256']),partial_proof_exact=True,prior100k_proof_prefix_exact=True)
    bumps[arm]=read_bumps((c/f'traces/{arm}.bumps.tsv').read_text(),window=tuple(m['bump_window']),cap=m['bump_row_cap'])
    content=(c/f'traces/{arm}.choice.tsv').read_text(); assert content and content.endswith('\n')
    choices[arm]=[line.split('\t') for line in content.splitlines()]
    assert all(len(row)==18 and row[0]==str(m['choice_conflict']) and row[4]==str(m['choice_variable']) for row in choices[arm])
def first_difference(fields):
    key=lambda row:tuple(row[k] for k in fields) if row is not None else None
    return next((dict(index=i,candidate=a,kissat=b) for i,(a,b) in enumerate(itertools.zip_longest(bumps['candidate'],bumps['kissat'])) if key(a)!=key(b)),None)
for p in (Path(__file__),root/'tools/bench/queue_bump_trace.py',root/'tools/bench/test_queue_bump_trace.py'): bind(p)
out=dict(observer_replay_validated=True,replay=replay,counts={a:len(rows) for a,rows in bumps.items()},
    first_move_identity_difference=first_difference(('conflicts','decisions','stable','variable')),
    first_links_values_difference=first_difference(('conflicts','decisions','stable','variable','previous','next','value')),
    all_bump_rows=bumps,raw_choice_rows=choices,hashes=hashes,
    limitation='Pre-move observations retain raw stamps/cursors and all duplicate moves. A uniform stamp offset is not silently removed; identity or link differences need source interpretation. Choice rows retain their source-defined18 fields. No runtime claim, whole-state equality or final proof acceptance.')
with target.open('x') as f: json.dump(out,f,indent=2); f.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('all_bump_rows','raw_choice_rows','hashes')},indent=2))
