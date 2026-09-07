#!/usr/bin/env python3
"""Predeclare stable-mode heap checkpoints from the retained identical prefix."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5403-rank35-direct-heap'
c = root / 'benchmarks/runs/20260907-5404-rank35-heap-bracket'
requested = [3005, 3071, 8000, 10553, 11837, 13564]
points = []
for target in requested:
    points.append(dict(requested_conflict=target))
count, digest, common = 0, 1469598103934665603, None
full = root / 'benchmarks/runs/20260907-5398-rank35-prefix/traces/candidate.decisions.tsv'
with full.open('rb') as stream:
    for line in stream:
        r = line.split(b'\t')
        conflict = int(r[0])
        for p in points:
            if 'snapshot_conflict' in p and conflict > p['snapshot_conflict'] and 'expected_count' not in p:
                p.update(expected_count=count, expected_hash=f'{digest:016x}', common_hash=f'{common:016x}')
        if all('expected_count' in p for p in points): break
        count += 1
        digest = ((digest ^ (int(r[3]) & ((1 << 64) - 1))) * 1099511628211) & ((1 << 64) - 1)
        if common is None: common = digest
        for p in points:
            if 'snapshot_conflict' not in p and conflict >= p['requested_conflict'] and int(r[4]) == 0:
                p.update(snapshot_conflict=conflict, snapshot_decisions=int(r[1]))
assert all('expected_count' in p for p in points)
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for a in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / a, c / 'pins' / a)
runner = (old / 'run.py').read_text()
runner = runner.replace("arm = ('candidate', 'kissat')[int(sys.argv[1])]", "task = int(sys.argv[1])\narm = ('candidate', 'kissat')[task % 2]\nlabel = f'{task // 2:02d}-' + arm")
runner = runner.replace("m = json.loads((c / 'manifest.json').read_text())", "m = json.loads((c / 'manifest.json').read_text())\npoint = m['points'][task // 2]\nm.update(stop_conflict=point['snapshot_conflict'], stream_from=point['snapshot_conflict'], choice_conflict=point['snapshot_conflict'], common_decisions=1)")
runner = runner.replace("(arm + '", "(label + '")
runner = runner.replace("glob(arm + '.*')", "glob(label + '.*')")
runner = runner.replace("'tactix-5403-'", "'tactix-5404-'")
runner = runner.replace("'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13832'", "'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': str(point['snapshot_conflict'])")
runner = runner.replace("'SAT_EVENT_TRACE_HEAP_SET_DECISION': '18441404'", "'SAT_EVENT_TRACE_HEAP_SET_DECISION': str(point['snapshot_decisions'])")
runner = runner.replace('r = dict(scratch=', 'r = dict(point=point, label=label, scratch=')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-1', '--array=0-11'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), points=points,
         hypothesis='Locate the earliest differing heap among six stable-mode checkpoints before the known Rank-35 choice split; observe existing source, never alter selection.',
         validation='12 clean stopped runs, exact full-prefix count/hash versus the retained common 5398 prefix, nonempty unique heap sets; compare portable membership, scores, assignments and array order. No complete verdict or timing claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(json.dumps(points, indent=2))
