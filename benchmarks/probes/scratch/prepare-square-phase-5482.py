#!/usr/bin/env python3
"""Reuse frozen observers to trace the first Square polarity split."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5471-square-bound-prefix'
c = root / 'benchmarks/runs/20260908-5482-square-phase'
summary = json.loads((old / 'summary.json').read_text())
assert summary['clean_stops'] == 2 and summary['matching_rows'] == 5848
first = summary['first_decision_difference']
assert first['candidate'] == ['501', '5849', '67', '-43773', '0']
assert first['kissat'] == ['501', '5849', '67', '43773', '0']
runner = (old / 'run.py').read_text()
anchor = "env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '0'"
assert runner.count(anchor) == 1
runner = runner.replace("'tactix-5471-'", "'tactix-5482-'")
runner = runner.replace(anchor, anchor + '''
env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.phase.tsv')),
            prefix + '_EVENT_TRACE_PHASE_ONLY': '1',
            prefix + '_EVENT_TRACE_CONFLICT_FROM': '0',
            prefix + '_EVENT_TRACE_CONFLICT_TO': '502',
            prefix + '_EVENT_TRACE_LIMIT': '100000',
            'SAT_EVENT_TRACE_PHASE_VAR': '43773'})
''')
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'):
    (c / name).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text(
    (old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         hypothesis='At first decision split both focused solvers choose variable 43773 at conflict 501, decision 5849, level 67, with opposite phases. Trace its saved-phase lifecycle from startup to conflict 502 to distinguish earlier assignment history from phase-choice logic. Later inprocessing is downstream, not an established cause.',
         validation='Require two clean stopped 10000-conflict streams identical to each respective 5471 arm, unchanged first split, valid pin/input/trace hashes, selected phase-save/pick records and no trace-limit truncation. This proves observer neutrality, not correctness or speed.',
         phase_window=[0, 502], phase_variable=43773,
         prior_summary_sha256=sha(old / 'summary.json'),
         test_consideration='No solver/observer implementation changes. Reuse existing tested phase observer; generated runner syntax and exact parent-stream/prefix replay guard this configuration-only diagnostic. A causal repair must add focused state-transition tests.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
