#!/usr/bin/env python3
"""Validate the changed-source causal replay against frozen native evidence."""
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from decision_prefix import read_prefix,validate_stream_accounting,first_stream_difference
from search_window import read_search
from multi_scan_watch import parse_watch
c=root/'benchmarks/runs/20260909-6735-reduce-watermark-prefix'
target=c/'summary-6739.json';assert not target.exists()
if not (c/'results/candidate.json').exists():
    print('Diagnostic pending; no summary');sys.exit(0)
m=json.loads((c/'manifest.json').read_text());hashes={}
def bind(p,expected=None):
    digest=sha(p);assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
for rel,digest in m['hashes'].items():bind(c/rel,digest)
for rel,digest in m['dependencies'].items():bind(root/rel,digest)
for name in ('manifest.json','launch.json','results/candidate.json'):bind(c/name)
r=json.loads((c/'results/candidate.json').read_text())
assert r['arm']=='candidate' and r['exit_code']==0 and r['scratch_deleted']
assert r['binary_sha256']==m['hashes']['pins/candidate']
assert r['partial_certificate']['bytes']>0
for name,digest in r['trace_sha256'].items():bind(c/'traces'/name,digest)
p=read_prefix(c/'traces/candidate.prefix.tsv')
assert p['conflict']==1000000 and p['common']==1 and p['common_reached']==1
first=last=None;count=0
with (c/'traces/candidate.decisions.tsv').open() as stream:
    for line in stream:
        assert line.endswith('\n')
        row=list(map(int,line.split('\t')))
        assert len(row)==5 and 0<=row[0]<=1000000 and row[2]>0 and row[3] and row[4] in (0,1)
        if first is None:first=row
        if last is not None:assert row[0]>=last[0]
        last=row;count+=1
validate_stream_accounting(p,first,last,count)
old=root/m['cached_controls']
differences={a:first_stream_difference(c/'traces/candidate.decisions.tsv',old/f'traces/{a}.decisions.tsv') for a in ('candidate','kissat')}
for d in differences.values():
    assert d['complete_equal'] or (d['left'] and d['right'] and min(d['left'][0],d['right'][0])>=62677)
window=tuple(m['search_window'])
rows={'new':read_search((c/'traces/candidate.search.tsv').read_text(),solver='tactix',window=window,cap=2000000)}
for a in ('candidate','kissat'):
    rows[a]=[x for x in read_search((old/f'traces/{a}.search.tsv').read_text(),solver='tactix' if a=='candidate' else 'kissat',window=(61030,73636),cap=2000000) if window[0]<=x['conflicts']<=window[1]]
key='18608,18610,18612,18614,18615,18617,18619,18620,18623,18626,18628,18630,18636,18659'
selected={a:[x for x in rr if parse_watch(x)['clause']==key] for a,rr in rows.items()}
def portable(row):
    if row is None:return None
    payload=parse_watch(row)
    return tuple(row[k] for k in ('conflicts','decisions','level','mode'))+tuple(payload.get(k) for k in ('scan','block','clause','searched','pose','values'))
def first_diff(left,right):
    return next((dict(index=i,left=a,right=b) for i,(a,b) in enumerate(itertools.zip_longest(left,right)) if portable(a)!=portable(b)),None)
watch_comparison={a:dict(total_rows=len(rows[a]),selected_rows=len(selected[a]),first_selected_difference=first_diff(selected['new'],selected[a]),first_global_difference=first_diff(rows['new'],rows[a])) for a in ('candidate','kissat')}
at={a:[x for x in rr if x['conflicts']==62680 and x['decisions']==245587] for a,rr in selected.items()}
assert all(len(rr)==1 for rr in at.values())
assert parse_watch(at['candidate'][0])['searched']=='6'
assert parse_watch(at['kissat'][0])['searched']=='2'
assert portable(at['new'][0])==portable(at['kissat'][0])
assert watch_comparison['kissat']['first_selected_difference'] is None
for path in (Path(__file__),root/'tools/bench/decision_prefix.py',root/'tools/bench/search_window.py',root/'tools/bench/multi_scan_watch.py',root/'tools/bench/watch_observer.py'):bind(path)
out=dict(rank=46,bounded_stop_clean=True,prefix=p,decision_difference=differences['kissat'],predecessor_difference=differences['candidate'],
    boundary_repaired=True,selected_clause=key,selected_rows=selected,watch_comparison=watch_comparison,hashes=hashes,
    limitation='Changed-source replay, not observer neutrality. All new1M decision rows accounted; frozen native reused without execution. Pre62677 decisions unchanged. The62680 promoted cursor and selected subsequent scans match native. Full runtime and final certificates remain separate gates; no speedup or full-trajectory claim unless decision_difference explicitly proves it.')
with target.open('x') as out_file:json.dump(out,out_file,indent=2);out_file.write('\n')
print(json.dumps({k:out[k] for k in ('bounded_stop_clean','boundary_repaired','decision_difference','predecessor_difference','watch_comparison')},indent=2))
