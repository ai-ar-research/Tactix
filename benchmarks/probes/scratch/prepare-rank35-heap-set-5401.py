#!/usr/bin/env python3
"""Freeze an existing-observer heap membership witness; no solver edits."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5400-window-validation'
c = root / 'benchmarks/runs/20260907-5401-rank35-heap-set'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'):
    (c / d).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('r = dict(arm=arm,', '''env.update({
    prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.events.tsv')),
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '13832',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '13832',
    prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
    prefix + '_EVENT_TRACE_SKIP_IMPLY': '1',
    prefix + '_EVENT_TRACE_LIMIT': '8',
    'SAT_EVENT_TRACE_HEAP_SET_PATH': str(c / 'traces' / (arm + '.heap.tsv')),
    'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13832',
    'SAT_EVENT_TRACE_HEAP_SET_DECISION': '18441404',
    'SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION': '1',
})
r = dict(arm=arm,''')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         hypothesis='At a common decision before the first Rank-35 choice split, distinguish differing heap membership from a permutation of equal-score variables.',
         snapshot_conflict=13832, snapshot_decisions=18441404,
         validation='Clean bounded stops, unchanged cumulative decision hashes versus 5400, exact pin hashes, nonempty heap snapshots, partial certificate cleanup; no timing or verdict claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
