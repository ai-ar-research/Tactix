#!/usr/bin/env python3
"""Capture the complete heap after the last common decision and wanted-variable state."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5869-rank32-reduce-frontier'
c = root / 'benchmarks/runs/20260908-5883-rank32-choice-frontier'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
v = json.loads((parent / 'reduce-validation.json').read_text())
assert v['comparison']['status'] == 'match'
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
runner = (parent / 'run.py').read_text().replace('tactix-5869-', 'tactix-5883-')
old = """env[prefix + '_REDUCE_TRACE'] = str(c / 'traces' / (arm + '.reduce.tsv'))
env[prefix + '_REDUCE_TRACE_CONFLICT_FROM'] = '225987'
env[prefix + '_REDUCE_TRACE_CONFLICT_TO'] = '225987'
"""
assert runner.count(old) == 1
extra = """env.update(SAT_DECISION_HEAP_SNAPSHOT='1',
           SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION='1',
           SAT_EVENT_TRACE_HEAP_SET_PATH=str(c / 'traces' / (arm + '.heap.tsv')),
           SAT_EVENT_TRACE_HEAP_SET_CONFLICT='225987',
           SAT_EVENT_TRACE_HEAP_SET_DECISION='61017804',
           SAT_CHOICE_STATE_CONFLICT='225987', SAT_CHOICE_STATE_VARIABLE='116746')
env[prefix + '_CHOICE_STATE_TRACE'] = str(c / 'traces' / (arm + '.choice.tsv'))
"""
(c / 'run.py').write_text(runner.replace(old, extra))
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('5869-reduce-frontier', '5883-choice-frontier'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), parent_campaign=str(parent.relative_to(root)),
    hypothesis='Reduction49 clause metadata and selected victims match exactly. Distinguish post-Reduce/Reorder heap scores/pose from propagation assignment drift immediately before first unequal choice61017805.',
    validation='Require both arms replay ordinary300K prefix,decisions,boundaries and partial proof bytes. Complete physical heap after common decision61017804;all116746 assignment/reason states at225987. Never infer root cause from unequal opaque refs.',
    native_rerun_reason='Cached native lacks complete heap and assignment reasons at the newly isolated decision boundary. Diagnostic only;fixed reference6810.066s remains unchanged.',
    test_consideration='No solver edit;existing standalone decision heap/choice observers. No broad event trace,no early-exit selector. Replay ordinary outputs before interpreting snapshot. Existing decision-prefix and heap protocol coverage retained.',
    reduce_evidence=dict(path=str((parent / 'reduce-validation.json').relative_to(root)), sha256=sha(parent / 'reduce-validation.json')))
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', c / 'pins/candidate', c / 'pins/kissat']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
