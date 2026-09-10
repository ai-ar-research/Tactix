#!/usr/bin/env python3
"""Localize accepted Rank21 search records without treating physical IDs as cause."""
import itertools
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from search_window import read_search
from compare_event_trace import normalize_semantic_rows
c = root/'benchmarks/runs/20260909-6782-rank21-trajectory-stop'
target = c/'comparison-6793.json'
assert not target.exists()
accepted = c/'summary-6784.json'
s = json.loads(accepted.read_text())
assert s['observer_replay_validated']
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest, rel
rows = {}
for arm, solver in (('candidate', 'tactix'), ('kissat', 'kissat')):
    raw = read_search((c/f'traces/{arm}.search.tsv').read_text(), solver=solver,
        window=(25098, 25103), cap=250000)
    rows[arm] = normalize_semantic_rows(raw)
fields = ('conflicts', 'decisions', 'level', 'mode', 'payload')
comparisons = {}
for event in ('conflict', 'analysis_begin', 'learned_raw', 'learned_sorted', 'learned_shrunk', 'learned', 'analysis_end', 'decision'):
    streams = {arm:[r for r in rr if r['event'] == event] for arm,rr in rows.items()}
    pairs = list(itertools.zip_longest(streams['candidate'], streams['kissat']))
    differences = [dict(index=i, candidate=a, kissat=b) for i,(a,b) in enumerate(pairs)
        if a is None or b is None or any(a[k] != b[k] for k in fields)]
    payload_equal = len(streams['candidate']) == len(streams['kissat']) and all(a['payload'] == b['payload'] for a,b in pairs)
    comparisons[event] = dict(counts={a:len(rr) for a,rr in streams.items()}, first=differences[0] if differences else None,
        all_payloads_equal=payload_equal)
    if event != 'decision':
        assert all(len(rr) == 6 for rr in streams.values())
        assert all(all(a[k] == b[k] for k in fields) for a,b in pairs[:5]), event
    if event.startswith('learned'):
        assert payload_equal, event
split = comparisons['decision']['first']
assert split['index'] == 35
a,b = split['candidate'],split['kissat']
assert all(a[k] == b[k] for k in ('conflicts', 'decisions', 'level', 'mode'))
assert (a['conflicts'], a['decisions'], a['level'], a['mode']) == (25102,25451310,14,'focused')
assert (a['payload'],b['payload']) == ('-402373','-402380')
contexts = {}
for arm in rows:
    contexts[arm] = [r for r in rows[arm] if r['event'] in ('decision','phase_pick','randec_state')
        and r['conflicts'] == 25102 and 25451308 <= r['decisions'] <= 25451311]
hashes = {str(p.relative_to(root)):sha(p) for p in (accepted,Path(__file__),
    root/'tools/bench/compare_event_trace.py', root/'tools/bench/search_window.py')}
out = dict(hashes=hashes, comparisons=comparisons, contexts=contexts,
    conclusion='All five pre-split conflict/analysis transactions25098..25102 match after existing physical-clause metadata normalization; all four learned-stage size/order/set digests match in all six transactions. First selected decision difference is25102/D25451310/L14, current-402373/native-402380.',
    next_action='Use existing same-pin selected-variable choice-state observer for external402380 at conflict25102. Compare assignment/reason and focused queue/cursor state around precedingD25451309 and splitD25451310, with exact original50K replay. This distinguishes a skipped implied variable from an already-displaced queue entry before adding broad traces.',
    limitation='Learned digests are bounded summaries, not a proof of whole-formula identity. Five matching local transactions do not exclude earlier latent queue/assignment differences. No implication/ranking snapshot in6782; raw propagation counters use different conventions. No cause or repair established.',
    test_consideration='Reuse existing tested search framing and physical-conflict normalization; assert every actual pre-split transaction field and all learned-stage payloads. Selected-variable successor requires strict schema and complete replay before interpretation.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(out['conclusion'])
print(out['next_action'])
