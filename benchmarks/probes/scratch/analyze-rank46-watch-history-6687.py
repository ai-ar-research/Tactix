#!/usr/bin/env python3
"""Locate the first complete portable-row difference in the validated history."""
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from search_window import read_search
from watch_observer import watch_payload
c=root/'benchmarks/runs/20260909-6684-rank46-watch-history'
target=c/'localization-6687.json'; assert not target.exists()
s=json.loads((c/'summary-6686.json').read_text()); assert s['observer_replay_validated']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
fields=('conflicts','decisions','level','mode')
payload_fields=('scan','block','clause','searched','pose','values')
def portable(r):
    if r is None: return None
    p=watch_payload(r['payload'],-16719)
    return tuple(r[k] for k in fields)+tuple(p.get(k) for k in payload_fields)
rows={a:read_search((c/f'traces/{a}.search.tsv').read_text(),solver='tactix' if a=='candidate' else 'kissat',window=(0,108341),cap=2000000) for a in ('candidate','kissat')}
first=next(dict(index=i,candidate=a,kissat=b) for i,(a,b) in enumerate(itertools.zip_longest(rows['candidate'],rows['kissat'])) if portable(a)!=portable(b))
assert first['index']==177292
assert first['candidate']['conflicts']==first['kissat']['conflicts']==108138
assert first['candidate']['decisions']==first['kissat']['decisions']==402320
previous={a:rows[a][first['index']-1] for a in rows}
selected={}
for label,key in s['selected_clause_keys'].items():
    streams={a:[r for r in rr if watch_payload(r['payload'],-16719)['clause']==key] for a,rr in rows.items()}
    discrepancy=next((dict(index=i,candidate=a,kissat=b) for i,(a,b) in enumerate(itertools.zip_longest(streams['candidate'],streams['kissat'])) if portable(a)!=portable(b)),None)
    selected[label]=dict(counts={a:len(rr) for a,rr in streams.items()},first_difference=discrepancy)
    if discrepancy:
        selected[label]['last_equal']={a:streams[a][discrepancy['index']-1] for a in streams} if discrepancy['index'] else None
out=dict(complete_portable_common_rows=first['index'],comparison_fields=fields,payload_fields=payload_fields,
    first_global_difference=first,previous_global_row=previous,selected_clauses=selected,
    hashes={str(p.relative_to(root)):sha(p) for p in (Path(__file__),c/'summary-6686.json',root/'tools/bench/search_window.py',root/'tools/bench/watch_observer.py')},
    conclusion='First177292 selected-scan portable rows agree from startup. At108138/402320/level19, the first difference is a32-literal clause pose swapping-18994 and18996, both false; clause identity,blocker,searched and values agree. The eventual21-literal conflict clause first differs at108162, after a matching observation108086. Both eventual conflict clauses initially agree afterBVE99580 and through102107; no basis to blame their creation or blindly reorder watches.',
    next_action='Capture complete conflict/implication detail108080..108170 on the same pins; require own1M decisions/partial proof replay. This brackets the first observed selected-scan pose change, not necessarily the earliest global divergence.',
    limitation='Only scans of propagated-16719 observed; positions are not matched across unmatched lists. Raw metadata, propagation-count conventions and branch-search hints are not part of this portable equality claim. Full1M trajectories remain different after108339.')
with target.open('x') as f: json.dump(out,f,indent=2); f.write('\n')
print(out['conclusion'])
for label,item in selected.items():
    d=item['first_difference']; print(label,item['counts'],None if d is None else (d['index'],d['candidate']['conflicts'],d['kissat']['conflicts']))
