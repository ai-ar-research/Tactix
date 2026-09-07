#!/usr/bin/env python3
"""Observe only selected root/heap lifecycle events near the narrowed split."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5403-rank35-direct-heap'
c = root / 'benchmarks/runs/20260907-5406-rank35-root-heap-events'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("'tactix-5403-'", "'tactix-5406-'")
runner = runner.replace("    'SAT_DECISION_HEAP_SNAPSHOT': '1',\n", "")
runner = runner.replace('r = dict(scratch=', '''env.update({
    prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.events.tsv')),
    prefix + '_EVENT_TRACE_HEAP_VARS_ONLY': '1',
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '13413',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '13415',
    prefix + '_EVENT_TRACE_LIMIT': '10000',
    'SAT_EVENT_TRACE_HEAP_VARS': '109797,109796,109877,109815',
})
r = dict(scratch=''')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), stop_conflict=13415,
         stream_from=13413, choice_conflict=13414, choice_variable=109797,
         common_decisions=1,
         hypothesis='Observe root-fixed 109797 retirement relative to selected heap pops/removals/reinsertions at conflicts 13413–13415, after the observed common heap at 13001.',
         validation='Two clean bounded stops, exact pin/trace hashes and partial-certificate cleanup, nonempty targeted event rows and identical common-prefix state. No completed-verdict or timing claim.')
m.pop('snapshot_conflict', None); m.pop('snapshot_decisions', None)
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
