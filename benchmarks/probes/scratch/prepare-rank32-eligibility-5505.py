#!/usr/bin/env python3
"""Observe current clauses and the preceding seed before native skips836."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5490-rank32-sweep'
c = root / 'benchmarks/runs/20260908-5505-rank32-sweep-eligibility'
authority = old / 'sweep-localization.json'
assert json.loads(authority.read_text())['calls'][0]['first_differences']['sweep_seed']['index'] == 14
runner = (old / 'run.py').read_text().replace("'tactix-5490-'", "'tactix-5505-'")
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''
selector = 'sweep_begin' if arm == 'kissat' else 'name=Sweep|stage=begin'
env.update({'SAT_EVENT_TRACE_BINARY_SET_PATH': str(c / 'traces' / (arm + '.binary.tsv')),
            'SAT_EVENT_TRACE_BINARY_SET_MATCH': selector,
            'SAT_EVENT_TRACE_BINARY_SET_CONFLICT': '19375',
            'SAT_EVENT_TRACE_BINARY_SET_EXTERNALS': '836,1236',
            'SAT_EVENT_TRACE_FORMULA_SET_PATH': str(c / 'traces' / (arm + '.formula.tsv')),
            'SAT_EVENT_TRACE_FORMULA_SET_MATCH': selector,
            'SAT_EVENT_TRACE_FORMULA_SET_CONFLICT': '19375',
            'SAT_EVENT_TRACE_FORMULA_SET_EXTERNALS': '836,1236',
            'SAT_EVENT_TRACE_FORMULA_SET_VALUES': '1',
            'SAT_EVENT_TRACE_SWEEP_QUERIES': '829',
            'SAT_EVENT_TRACE_SWEEP_VARS': '829'})
''' + anchor)
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
         selected_authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority)),
         hypothesis='After14 matching seed/environment-size/tick-delta/solve records, Tactix alone processes836. Native refreshes retained eligibility; capture selected836/1236 binary and large clauses at first Sweep entry plus preceding829 queries/variables to distinguish eligibility from an earlier in-environment representative change.',
         validation='Two clean stops and exact entire per-arm decision/prefix/boundary replay against5490. Hash all traces/pins; require first-entry selected snapshots, preserve both native/Tactix boundary formats and any physical multiplicity. Live clause snapshots alone cannot establish physical watch eligibility or active identity.',
         test_consideration='No solver code change. Existing snapshot/window fixtures, runner syntax and exact full parent replay guard observers. A repair needs a focused retained-eligibility state-transition fixture.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
