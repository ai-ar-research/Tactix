#!/usr/bin/env python3
"""Capture Sweep's first differing Rank-32 invocation on existing trace pins."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5479-rank32-bound-prefix'
c = root / 'benchmarks/runs/20260908-5490-rank32-sweep'
localization = json.loads((old / 'boundary-localization.json').read_text())
first = localization['first_coarse_boundary']
assert first['index'] == 89 and first['candidate']['name'] == 'Sweep'
assert first['candidate']['conflicts'] == first['kissat']['conflicts'] == 19375
runner = (old / 'run.py').read_text().replace("'tactix-5479-'", "'tactix-5490-'")
anchor = "env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '0'"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, anchor + '''
env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.sweep.tsv')),
            prefix + '_EVENT_TRACE_SWEEP_ONLY': '1',
            prefix + '_EVENT_TRACE_CONFLICT_FROM': '19375',
            prefix + '_EVENT_TRACE_CONFLICT_TO': '19375',
            prefix + '_EVENT_TRACE_LIMIT': '2000000'})
if arm == 'candidate':
    env['TACTIX_SLOT_TRACE'] = '1'
''')
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'):
    (c / name).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         stop_conflict=19376, stream_from=0,
         localization_sha256=sha(old / 'boundary-localization.json'),
         hypothesis='At common coarse Sweep entry19375, Tactix exits with one more active variable and four fewer binaries; the first decision split follows at the same conflict. Compare schedule, environments, queries, units/equivalences and effort before declaring a missed result or changing budgets.',
         validation='Two clean stops immediately beyond selected Sweep, complete recorded prefixes exactly replay each5479 parent stream to the stop, immutable hashes and no selected-event truncation. Compare portable Sweep records; no complete verdict, proof check or timing acceptance.',
         test_consideration='No solver/observer change. Generated runner syntax and full observed parent-prefix replay guard this narrower configuration. Inherited event-trace/Sweep fixtures remain; a repair must add exact-transition coverage.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
