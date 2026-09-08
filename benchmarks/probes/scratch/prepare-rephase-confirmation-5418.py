#!/usr/bin/env python3
"""Confirm the repaired warm-up ordering and formerly divergent Rank-35 prefix."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5415-rank35-warmup-implications'
c = root / 'benchmarks/runs/20260907-5418-rank35-rephase-confirmation'
authority = root / 'benchmarks/runs/20260907-5417-rephase-root/manifest.json'
assert authority.exists()
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
shutil.copy2(root / 'build/pin-5417-rephase-root-trace/tactix', c / 'pins/candidate')
shutil.copy2(old / 'pins/kissat', c / 'pins/kissat')
runner = (old / 'run.py').read_text().replace("'tactix-5415-'", "'tactix-5418-'")
runner = runner.replace('r = dict(scratch=', """env['SAT_DECISION_HEAP_SNAPSHOT'] = '1'
env['SAT_EVENT_TRACE_HEAP_SET_PATH'] = str(c / 'traces' / (arm + '.heap.tsv'))
env['SAT_EVENT_TRACE_HEAP_SET_CONFLICT'] = '13563'
env['SAT_EVENT_TRACE_HEAP_SET_DECISION'] = '17798664'
env['SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION'] = '1'
r = dict(scratch=""")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
         candidate_source='.claude/worktrees/runtime-rephase-root-5417', stop_conflict=13833, common_decisions=18516149,
         snapshot_conflict=13563, snapshot_decisions=17798664,
         hypothesis='The missing root closure caused the first warm-up implication-order difference, later heap permutation, and first decision split at 13832. Restore it without changing public rules.',
         validation='Two clean stops and pin/trace cleanup; native reproduces established 13833 prefix; compare complete post-warm-up heap portable fields, six selected implications and full cumulative decision hash. No timing claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
