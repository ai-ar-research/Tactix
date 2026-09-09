#!/usr/bin/env python3
"""Require unchanged full replay, then account the selected conflict watch order."""
from collections import Counter
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from search_window import read_search
from watch_observer import watch_payload, watch_replay
c = root/'benchmarks/runs/20260909-6681-rank46-conflict-watch'
target = c/'summary-6683.json'; assert not target.exists()
m = json.loads((c/'manifest.json').read_text()); hashes = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected,p
    hashes[str(p.relative_to(root))] = digest
for rel,digest in m['hashes'].items(): bind(c/rel,digest)
for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
for name in ('manifest.json','launch.json'): bind(c/name)
scan = m['selected_scan_literal']; window = tuple(m['search_window'])
keys = {arm:dict(part.split('=',1) for part in records[0]['payload'].split('|'))['set']
        for arm,records in m['conflict_records'].items()}
rows = {}; replay = {}; selected = {}; hits = {}
for arm in ('candidate','kissat'):
    p=c/f'results/{arm}.json'; bind(p); r=json.loads(p.read_text())
    parent=root/m['cached_controls'][arm]; old=json.loads((parent/f'results/{arm}.json').read_text())
    assert r['exit_code']==0 and r['scratch_deleted'] and r['arm']==arm
    assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
    assert r['partial_certificate']==old['partial_certificate']
    assert r['prior_certificate_prefix']==old['prior_certificate_prefix']
    assert r['trace_sha256'].keys()==old['trace_sha256'].keys()
    for name,digest in r['trace_sha256'].items():
        bind(c/'traces'/name,digest)
        if name != arm+'.search.tsv': assert digest==old['trace_sha256'][name],name
    kw=dict(solver='tactix' if arm=='candidate' else 'kissat',window=window,cap=m['trace_row_cap'])
    before=read_search((parent/f'traces/{arm}.search.tsv').read_text(),**kw)
    after=read_search((c/f'traces/{arm}.search.tsv').read_text(),**kw)
    extras=watch_replay(before,after,scan=scan,window=window)
    replay[arm]=dict(original_rows=len(before),total_rows=len(after),watch_rows=len(extras),
                     exact_standalone=[n for n in r['trace_sha256'] if n!=arm+'.search.tsv'])
    selected[arm]=[r for r in extras if r['conflicts']==108337 and r['decisions']==403136 and r['level']==18]
    assert selected[arm]
    hits[arm]={label:[dict(position=i,row=r) for i,r in enumerate(selected[arm])
                         if watch_payload(r['payload'],scan)['clause']==key] for label,key in keys.items()}
    for label,rr in hits[arm].items():
        assert len(rr)==1,(arm,label)
        fields=watch_payload(rr[0]['row']['payload'],scan)
        assert set(fields['values'].split(','))=={'-1'}
        assert fields.get('garbage','0')=='0'
        assert fields['block']=='16859'
    rows[arm]=extras
assert hits['candidate']['candidate'][0]['position'] < hits['candidate']['kissat'][0]['position']
assert hits['kissat']['kissat'][0]['position'] < hits['kissat']['candidate'][0]['position']
lists={a:[watch_payload(r['payload'],scan)['clause'] for r in rr] for a,rr in selected.items()}
one_sided={a:list((Counter(lists[a])-Counter(lists[b])).elements()) for a,b in (('candidate','kissat'),('kissat','candidate'))}
for p in (Path(__file__),root/'tools/bench/watch_observer.py',root/'tools/bench/test_watch_observer.py',root/'tools/bench/search_window.py'): bind(p)
out=dict(rank=46,stop=1000000,observer_replay_validated=True,replay=replay,selected_scan=scan,
    selected_preconflict=108337,selected_decision=403136,selected_level=18,
    selected_list_counts={a:len(rr) for a,rr in selected.items()},conflict_hits=hits,
    one_sided_clause_multisets=one_sided,all_added_watch_rows=rows,hashes=hashes,
    conclusion='Both conflicting clauses exist and are already all-false in both selected lists. Their relative physical order is reversed; each solver selects its earlier clause. The differing learned clause therefore follows conflict selection, not shrink arithmetic or the later branch queue. Trace counters precede the conflict increment: watch108337 leads to conflict108338.',
    limitation='First differing watch movement or reconstruction remains unlocalized. One-sided entries and pose differences are evidence, not permission to sort or restore a clause. No runtime claim or final-proof acceptance from stopped diagnostics.')
with target.open('x') as f: json.dump(out,f,indent=2); f.write('\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('hashes','all_added_watch_rows','conflict_hits')},indent=2))
