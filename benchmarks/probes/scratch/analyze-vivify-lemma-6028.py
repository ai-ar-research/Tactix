#!/usr/bin/env python3
"""Localize learned-clause presence changes separately from physical pose."""
import importlib.util
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6024-vivify-lemma-history'
sp = importlib.util.spec_from_file_location('series', Path(__file__).with_name('validate-clause-pose-series-6009.py'))
mod = importlib.util.module_from_spec(sp); sp.loader.exec_module(mod)
key = {53, 51, -47, -93630, -4166, -4349, 4172}
summary = json.loads((c / 'summary.json').read_text()); assert summary['full_replay']
target = c / 'presence-analysis.json'; assert not target.exists()
arms, transitions, hashes = {}, {}, {}
for arm in ('candidate', 'kissat'):
    p = c / 'traces' / (arm + '.clause-pose-series.tsv')
    hashes[p.name] = mod.sha(p)
    assert hashes[p.name] == summary['evidence'][arm]['trace_sha256'][p.name]
    with p.open() as stream: frames = mod.parse(stream, key)
    previous = None; state = None; transitions[arm] = []; arms[arm] = {}
    occurrences = {}
    for frame in frames:
        base = (frame['conflicts'], frame['name'], frame['stage'])
        occurrence = occurrences.get(base, 0); occurrences[base] = occurrence + 1
        arms[arm][base + (occurrence,)] = frame
        new_state = sorted(x['redundant'] for x in frame['clauses'])
        if state != new_state:
            transitions[arm].append(dict(before=previous, after=frame)); state = new_state
        previous = frame
common_before = None; first = None
for key, a in arms['candidate'].items():
    if key not in arms['kissat']: continue
    b = arms['kissat'][key]
    if sorted(x['redundant'] for x in a['clauses']) != sorted(x['redundant'] for x in b['clauses']):
        first = dict(key=key, previous_common=common_before, candidate=a, kissat=b); break
    common_before = dict(key=key, candidate=a, kissat=b)
out = dict(full_replay_summary_sha256=mod.sha(c / 'summary.json'), trace_sha256=hashes,
    presence_transitions=transitions, first_common_presence_difference=first,
    limitation='Canonical learned clause presence at sampled boundaries. A transition between different phases may be search creation/deletion between them, not either named phase. No unsoundness, weaker-work or runtime improvement follows from unequal redundant clauses alone.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(out, indent=2))
