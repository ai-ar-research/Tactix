#!/usr/bin/env python3
"""Locate analysis-stream differences only after the original strict replay."""
from collections import Counter
import itertools
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from search_window import read_search
from compare_event_trace import normalize_semantic_rows
c=root/'benchmarks/runs/20260910-6804-rank21-analysis-admission'
target=c/'comparison-6811.json'
assert not target.exists()
hashes={}
def bind(p,expected=None):
    digest=sha(p);assert expected is None or digest==expected,p
    hashes[str(p.relative_to(root))]=digest
classification=c/'configuration-6810.json'
classified=json.loads(classification.read_text());bind(classification)
for rel,digest in classified['hashes'].items():bind(root/rel,digest)
rows={}; windows={}; summaries={}
events=('conflict','analysis_begin','analysis_mark','analysis_resolve',
    'analysis_binary_reason','analysis_jumped_over','analysis_reason_size',
    'analysis_reason_lit','analysis_tick_reason_bump','reason_bump_delay_before',
    'reason_bump_rate_milli','learned_raw','learned_sorted','learned_shrunk','learned','analysis_end')
fields=('conflicts','decisions','level','mode','payload')
for window in ('early','late'):
    sub=c/window;accepted=sub/'summary-6806.json';bind(accepted)
    s=json.loads(accepted.read_text());assert s['observer_replay_validated']
    for rel,digest in s['hashes'].items():bind(root/rel,digest)
    m=json.loads((sub/'manifest.json').read_text());scope=tuple(m['search_window'])
    window_rows={}
    for arm,solver in (('candidate','tactix'),('kissat','kissat')):
        raw=read_search((sub/f'traces/{arm}.search.tsv').read_text(),solver=solver,
            window=scope,cap=m['trace_row_cap'])
        window_rows[arm]=normalize_semantic_rows(raw)
    comparisons={}
    for event in events:
        streams={arm:[r for r in rr if r['event']==event and scope[0]<=r['conflicts']<=scope[1]]
                 for arm,rr in window_rows.items()}
        pairs=list(itertools.zip_longest(streams['candidate'],streams['kissat']))
        key=lambda r:tuple(r[f] for f in fields) if r is not None else None
        difference=next((dict(index=i,candidate=a,kissat=b)
            for i,(a,b) in enumerate(pairs) if key(a)!=key(b)),None)
        comparisons[event]=dict(counts={a:len(rr) for a,rr in streams.items()},first=difference)
    windows[window]=dict(comparisons=comparisons,
        counts={a:dict(Counter(r['event'] for r in rr)) for a,rr in window_rows.items()})
    rows[window]=window_rows
    print(window,json.dumps({e:v for e,v in comparisons.items() if v['first']},indent=2))
for p in (Path(__file__),root/'tools/bench/search_window.py',root/'tools/bench/compare_event_trace.py'):bind(p)
out=dict(hashes=hashes,windows=windows,rows=rows,
    configuration='6810 records the misspelled snapshot-suppression setting. This comparator requires complete strict6806 replay despite that overhead; it does not reclassify the setting as effective.',
    limitation='Separate event streams retain raw order and exact conflict/decision/level clocks; only existing conflict physical-ID/header normalization applies. A first raw resolution or reason mismatch is not yet a proved solver defect or weaker effect. Native has no analyzed payload event. No final proof acceptance or runtime claim.',
    test_consideration='Existing search framing/normalization tests and strict full6798 replay are prerequisites. This read-only comparison adds no parser or solver behavior; missing events and unequal counts remain explicit rather than filtered away.')
with target.open('x') as stream:json.dump(out,stream,indent=2);stream.write('\n')
print(target)
