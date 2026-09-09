#!/usr/bin/env python3
"""Explain the victim-count change using complete candidate multisets."""
from collections import Counter
import importlib.util
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6038-learned-retention'
target = c / 'population-analysis.json'; assert not target.exists()
summary = json.loads((c / 'summary.json').read_text()); assert summary['full_parent_replay']
sp = importlib.util.spec_from_file_location('reduce', root / 'tools/bench/compare_reduce_trace.py')
mod = importlib.util.module_from_spec(sp); sp.loader.exec_module(mod)
tables = {a: mod.load(c / 'traces' / (a + '.reduce.tsv')) for a in ('candidate','kissat')}
out = []
for reduction in sorted(tables['candidate'][0]):
    rows = {a: [r for r in t[1] if r['reduction'] == reduction] for a,t in tables.items()}
    counts = {a: Counter(r['literals'] for r in rr) for a,rr in rows.items()}
    extra = {a: counts[a] - counts[b] for a,b in [('candidate','kissat'),('kissat','candidate')]}
    selected = {a: [r for r in rr if r['literals'] in extra[a]] for a,rr in rows.items()}
    ranked = {a: sorted((r for r in rr if r['rank'] is not None), key=lambda r:r['rank']) for a,rr in rows.items()}
    for a, rr in ranked.items(): assert [r['rank'] for r in rr] == list(range(len(rr)))
    shared = counts['candidate'] & counts['kissat']
    projected = {}
    for a, rr in ranked.items():
        remaining = shared.copy(); projected[a] = []
        for r in rr:
            if remaining[r['literals']]:
                projected[a].append(r['literals']); remaining[r['literals']] -= 1
    out.append(dict(reduction=reduction, only_rows=selected,
        candidate_only_copies=sum(extra['candidate'].values()), kissat_only_copies=sum(extra['kissat'].values()),
        ranked_common_projection_equal=projected['candidate'] == projected['kissat'],
        summaries={a: t[0][reduction] for a,t in tables.items()}))
target.write_text(json.dumps(dict(reductions=out,
    limitation='Candidate-rank projection excludes unmatched physical copies while preserving remaining order. This does not show why the populations differ or justify changing the reduction fraction.'), indent=2)+'\n')
print(json.dumps(out, indent=2))
