#!/usr/bin/env python3
"""Retain complete semantic mismatches at the first changed victim set."""
from collections import Counter, defaultdict
import importlib.util
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
sp = importlib.util.spec_from_file_location('history', Path(__file__).with_name('validate-retention-history-6044.py'))
h = importlib.util.module_from_spec(sp); sp.loader.exec_module(h)
c = h.C; target=c/'reason-frontier.json'; assert not target.exists()
assert json.loads((c/'summary.json').read_text())['full_parent_replay']
out=[]
for a,b in zip(h.reductions(c/'traces/candidate.reduce.tsv'),h.reductions(c/'traces/kissat.reduce.tsv')):
    if a[0]['reduction'] > 51: break
    counts=[Counter(r['literals'] for r in t[1]) for t in (a,b)]
    assert counts[0] == counts[1]
    victims=[Counter(r['literals'] for r in t[2] if r['selected']) for t in (a,b)]
    semantic_fields=('literals','size','glue','used','reason','eligibility')
    semantic=[Counter(tuple(r[k] for k in semantic_fields) for r in t[1]) for t in (a,b)]
    if semantic[0] == semantic[1] and victims[0] == victims[1]: continue
    differences=[semantic[0]-semantic[1],semantic[1]-semantic[0]]
    keys={key[0] for diff in differences for key in diff}
    rows={arm:[r for r in t[1] if r['literals'] in keys] for arm,t in zip(('candidate','kissat'),(a,b))}
    candidate_keys=[set(r['literals'] for r in t[2]) for t in (a,b)]
    only_candidate_keys=candidate_keys[0]^candidate_keys[1]
    out.append(dict(reduction=a[0]['reduction'],conflict=a[0]['conflict'],
        summaries={'candidate':a[0],'kissat':b[0]}, semantic_mismatches=rows,
        candidate_membership_witness={arm:[r for r in rr if r['literals'] in only_candidate_keys] for arm,rr in rows.items()},
        victim_sets_equal=victims[0]==victims[1],
        victim_only={arm:[dict(literals=k,multiplicity=n) for k,n in diff.items()]
                     for arm,diff in zip(('candidate','kissat'),(victims[0]-victims[1],victims[1]-victims[0]))}))
assert out and next(r for r in out if not r['victim_sets_equal'])['reduction'] == 51
sources=[root/'.claude/worktrees/runtime-bve-active-pop-5962/src/rules/reduce/reduce.cpp',
         root/'.claude/worktrees/runtime-bve-active-pop-5962/src/kernel/kernel.cpp',
         root/'tools/solvers/variants/kissat-learned-stage-6030/src/trail.c']
result=dict(summary_sha256=h.sha(c/'summary.json'),boundaries=out,
    source_sha256={str(p.relative_to(root)):h.sha(p) for p in sources},
    inference='Kissat flushes the root trail before reason locking; Tactix retains root identities and lock_reasons scans every trail assignment without excluding level0. At51 exactly one differently protected clause changes candidate membership. A fixed root-reason/unit-proof fixture is the next required implementation witness; this trace alone does not record assignment levels.',
    next_action='On a new5962 successor, test root reason candidate eligibility and proof-safe retirement, preserving pending and non-root reason locks. Require native/candidate caller fixtures, checked DRAT/LRAT, full causal replay, then B28+C5/full40.')
target.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in out[-1].items() if k!='semantic_mismatches'},indent=2))
