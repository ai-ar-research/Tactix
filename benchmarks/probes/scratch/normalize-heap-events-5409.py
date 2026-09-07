#!/usr/bin/env python3
"""Separate observer call-site counters from portable heap mutations."""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5408-rank35-pre-rephase-heap'
events = {}
for arm in ('candidate', 'kissat'):
    events[arm] = [dict(event=r[2], conflict=int(r[3]), mode=r[7],
                        payload=re.sub(r'\|trail_after=\d+', '', r[-1]))
                   for line in (c / 'traces' / (arm + '.events.tsv')).read_text().splitlines()
                   if not line.startswith('#') and int((r := line.split('\t'))[3]) >= 13414]
t, k = events['candidate'], events['kissat']
assert len(t) == len(k) == 166
diffs = [dict(index=i, candidate=a, kissat=b) for i, (a, b) in enumerate(zip(t, k)) if a != b]
result = dict(selected_event_count=len(t), portable_event_difference_count=len(diffs),
              first_differences=diffs[:5],
              counter_scope='Kissat decide.c increments level and ordinary decision count before picking; Tactix default_policy.cpp picks before applying Decide. Warm-up increments level but not ordinary decisions. Raw 5408 comparison retains these offsets; this comparison drops both counters, retaining event, conflict, mode and full payload.',
              sources=['tools/solvers/variants/kissat-direct-heap-5402/src/decide.c:213', '.claude/worktrees/runtime-direct-heap-5402/src/policies/default_policy.cpp:2738', '.claude/worktrees/runtime-direct-heap-5402/src/policies/default_policy.cpp:2796'],
              limitation='Selected variables only; equality does not establish equality of all heap updates.')
p = c / 'portable-event-summary.json'
if p.exists(): assert json.loads(p.read_text()) == result
else: p.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
