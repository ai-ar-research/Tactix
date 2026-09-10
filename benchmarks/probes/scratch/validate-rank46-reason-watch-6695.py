#!/usr/bin/env python3
"""Replay-check the selected reason watch list without assuming its contents."""
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from search_window import read_search
from watch_observer import watch_replay,watch_payload
c=root/'benchmarks/runs/20260909-6693-rank46-reason-watch'
target=c/'summary-6695.json'; assert not target.exists()
if any(not (c/f'results/{a}.json').exists() for a in ('candidate','kissat')):
    print('Pending diagnostic; no summary'); sys.exit(0)
m=json.loads((c/'manifest.json').read_text()); hashes={}
def bind(p,expected=None):
    digest=sha(p); assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
replay={}; watches={}; hits={}
for a in ('candidate','kissat'):
    p=c/f'results/{a}.json'; bind(p); r=json.loads(p.read_text())
    parent=root/m['cached_controls'][a]; old=json.loads((parent/f'results/{a}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==a
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+a]
    for key in ('partial_certificate','prior_certificate_prefix'): assert r[key]==old[key]
    assert r['trace_sha256'].keys()==old['trace_sha256'].keys()
    for name,digest in r['trace_sha256'].items():
        bind(c/'traces'/name,digest)
        if name!=a+'.search.tsv': assert digest==old['trace_sha256'][name],name
    kw=dict(solver='tactix' if a=='candidate' else 'kissat',window=tuple(m['search_window']),cap=m['trace_row_cap'])
    before=read_search((parent/f'traces/{a}.search.tsv').read_text(),**kw)
    after=read_search((c/f'traces/{a}.search.tsv').read_text(),**kw)
    extras=watch_replay(before,after,scan=m['selected_scan_literal'],window=tuple(m['search_window']))
    replay[a]=dict(original_rows=len(before),total_rows=len(after),added_watch_rows=len(extras),standalone_exact=True)
    watches[a]=extras
    selected=[row for row in extras if row['conflicts']==m['selected_conflict'] and row['decisions']==m['selected_decision'] and row['level']==m['selected_level']]
    assert selected,(a,'no selected state')
    hits[a]={label:[dict(position=i,row=row,fields=watch_payload(row['payload'],m['selected_scan_literal'])) for i,row in enumerate(selected)
                   if watch_payload(row['payload'],m['selected_scan_literal'])['clause']==key] for label,key in m['reason_keys'].items()}
for p in (Path(__file__),root/'tools/bench/watch_observer.py',root/'tools/bench/search_window.py'): bind(p)
out=dict(observer_replay_validated=True,rank=46,replay=replay,reason_hits=hits,all_added_watch_rows=watches,hashes=hashes,
    limitation='Selected state hits retain all occurrences, including duplicate or absent reasons. Values are pre-scan snapshots; reason eligibility/physical order require explicit interpretation. Stopped diagnostic is not final proof acceptance or timing evidence.')
with target.open('x') as f: json.dump(out,f,indent=2); f.write('\n')
print(json.dumps(dict(replay=replay,reason_hits=hits),indent=2))
