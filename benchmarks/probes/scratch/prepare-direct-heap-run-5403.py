#!/usr/bin/env python3
"""Pin direct snapshots and make cancellation cleanup explicit for this runner."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5400-window-validation'
c = root / 'benchmarks/runs/20260907-5403-rank35-direct-heap'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'):
    (c / d).mkdir()
for arm, binary in [('candidate', 'build/pin-5402-direct-heap/tactix'),
                    ('kissat', 'tools/solvers/variants/kissat-direct-heap-5402/build/kissat')]:
    shutil.copy2(root / binary, c / 'pins' / arm)
runner = (old / 'run.py').read_text()
runner = runner.replace('import shutil\n', 'import shutil\nimport signal\n')
runner = runner.replace("prefix='tactix-rank27-prefix-',", "prefix='tactix-5403-' + os.environ['SLURM_JOB_ID'] + '-',")
runner = runner.replace('r = dict(arm=arm,', '''env.update({
    'SAT_DECISION_HEAP_SNAPSHOT': '1',
    'SAT_EVENT_TRACE_HEAP_SET_PATH': str(c / 'traces' / (arm + '.heap.tsv')),
    'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13832',
    'SAT_EVENT_TRACE_HEAP_SET_DECISION': '18441404',
    'SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION': '1',
})
def interrupted(signum, frame):
    raise SystemExit(128 + signum)
signal.signal(signal.SIGTERM, interrupted)
signal.signal(signal.SIGINT, interrupted)
print('scratch=' + str(scratch), flush=True)
r = dict(scratch=str(scratch), arm=arm,''')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         candidate_authority='benchmarks/runs/20260907-5402-direct-heap-observer/manifest.json',
         candidate_source='.claude/worktrees/runtime-direct-heap-5402',
         snapshot_conflict=13832, snapshot_decisions=18441404,
         hypothesis='At a common post-decision boundary before the Rank-35 choice split, compare heap membership, score, assignment and position using a direct one-shot observer.',
         validation='Two clean bounded stops; nonempty snapshots; exact decision windows and full cumulative hashes versus 5400; all immutable pin/trace hashes; certificate cleanup. No solve or timing claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
