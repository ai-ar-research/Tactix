#!/usr/bin/env python3
"""Join first population witnesses to full learning history and prior survivors."""
from collections import Counter
import importlib.util
import itertools
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
sp = importlib.util.spec_from_file_location('history', Path(__file__).with_name('validate-retention-history-6044.py'))
h = importlib.util.module_from_spec(sp); sp.loader.exec_module(h)
c = h.C; target = c / 'origin-analysis.json'; assert not target.exists()
summary = json.loads((c/'summary.json').read_text()); assert summary['full_parent_replay']
first_population = next(r for r in summary['reductions'] if r['candidate_only_copies'] or r['kissat_only_copies'])
keys = {tuple(r['literals']) for item in summary['reductions'] for rows in item['only_rows'].values() for r in rows}
births = {arm: [] for arm in ('candidate','kissat')}
first_stage_difference = {}
with (c/'traces/candidate.learned-stage.tsv').open() as a, (c/'traces/kissat.learned-stage.tsv').open() as b:
    streams = [itertools.groupby(h.learned(f), lambda r:r[1]) for f in (a,b)]
    current = [next(s,None) for s in streams]
    while any(r is not None for r in current):
        conflict = min(r[0] for r in current if r is not None)
        groups = []
        for i,arm in enumerate(('candidate','kissat')):
            rows = list(current[i][1]) if current[i] is not None and current[i][0] == conflict else []
            if rows: current[i] = next(streams[i],None)
            groups.append(rows)
            for r in rows:
                if tuple(sorted(r[-1])) in keys: births[arm].append(r)
        for event in ('learned_raw','learned_sorted','learned_shrunk','learned'):
            if event in first_stage_difference: continue
            normalized = [[(r[1:5],tuple(sorted(r[-1]))) for r in rows if r[0] == event] for rows in groups]
            if normalized[0] != normalized[1]:
                first_stage_difference[event] = dict(conflict=conflict,candidate=groups[0],kissat=groups[1])
history=[]; previous_survivors=None
for a,b in zip(h.reductions(c/'traces/candidate.reduce.tsv'),h.reductions(c/'traces/kissat.reduce.tsv')):
    counts = [Counter(r['literals'] for r in t[1]) for t in (a,b)]
    survivors = [Counter(r['literals'] for r in t[1] if not r['selected']) for t in (a,b)]
    incoming = [counts[0]-counts[1],counts[1]-counts[0]]
    outgoing = [survivors[0]-survivors[1],survivors[1]-survivors[0]]
    if any(incoming) or any(outgoing):
        def render(cs): return {arm:[dict(literals=k,multiplicity=v) for k,v in cnt.items()] for arm,cnt in zip(('candidate','kissat'),cs)}
        history.append(dict(reduction=a[0]['reduction'], conflict=a[0]['conflict'],
            incoming=render(incoming), outgoing=render(outgoing),
            incoming_equals_previous_survivor_difference=previous_survivors == incoming))
    previous_survivors=outgoing
out = dict(summary_sha256=h.sha(c/'summary.json'),first_population=first_population,
           witness_learning_stages=births,first_learning_difference_by_stage=first_stage_difference,
           population_transitions=history,
           limitation='Survivor difference equality establishes multiset continuity only, not absence of cancelling changes. Learned-stage differences may be callback timing, requiring real clause and reason witnesses.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
