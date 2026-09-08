#!/usr/bin/env python3
"""Pin a narrow implication witness for the divergent warm-up trail order."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5410-rank35-first-warmup-heap'
c = root / 'benchmarks/runs/20260907-5411-rank35-warmup-implications'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("'tactix-5410-'", "'tactix-5411-'")
runner = runner.replace("env['SAT_DECISION_HEAP_SNAPSHOT'] = '1'", """env.pop(prefix + '_EVENT_TRACE_HEAP_VARS_ONLY', None)
for suffix in ('SKIP_HEAP', 'SKIP_WATCH', 'SKIP_DECISIONS', 'SKIP_RANDEC_DRAWS', 'SKIP_ANALYSIS_TICKS', 'IMPLY_ONLY'):
    env[prefix + '_EVENT_TRACE_' + suffix] = '1'
env[prefix + '_EVENT_TRACE_CONFLICT_FROM'] = '13563'
env['SAT_EVENT_TRACE_IMPLY_LITERALS'] = '109796,109815,109877,109841'
for key in list(env):
    if key.startswith('SAT_EVENT_TRACE_HEAP_SET_'):
        del env[key]""")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         hypothesis='Compare selected implication levels, order and reason contents during warm-up 13563; locate the producer before different heap reinsertions.',
         validation='Two clean stopped prefixes identical to 5410, identical decision streams, pin/trace hashes and scratch cleanup, nonempty selected implication rows; no timing claim.')
for key in ('snapshot_conflict', 'snapshot_decisions'): m.pop(key)
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
