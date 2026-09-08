#!/usr/bin/env python3
"""Capture the selected auxiliary's startup clauses with unchanged observers."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5487-square-implications'
c = root / 'benchmarks/runs/20260908-5496-square-definition'
summary = json.loads((old / 'summary.json').read_text())
assert summary['clean_stops'] == 2 and summary['matching_rows'] == 5848
runner = (old / 'run.py').read_text().replace("'tactix-5487-'", "'tactix-5496-'")
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''
env.update({'SAT_EVENT_TRACE_BINARY_SET_PATH': str(c / 'traces' / (arm + '.binary.tsv')),
            'SAT_EVENT_TRACE_BINARY_SET_MATCH': 'stage=end',
            'SAT_EVENT_TRACE_BINARY_SET_CONFLICT': '0',
            'SAT_EVENT_TRACE_BINARY_SET_EXTERNALS': '43773',
            'SAT_EVENT_TRACE_FORMULA_SET_PATH': str(c / 'traces' / (arm + '.formula.tsv')),
            'SAT_EVENT_TRACE_FORMULA_SET_MATCH': 'name=' + ('factor' if arm == 'kissat' else 'Factor') + '|stage=end',
            'SAT_EVENT_TRACE_FORMULA_SET_CONFLICT': '0',
            'SAT_EVENT_TRACE_FORMULA_SET_EXTERNALS': '43773'})
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
         prior_summary_sha256=sha(old / 'summary.json'),
         hypothesis='Validated5487 shows negative43773 from binary(-43773,-20046), while native repeatedly assigns positive from(3270,43773). Capture selected binary multisets at startup end boundaries and selected large clauses at first Factor exit before assuming equal auxiliary definitions.',
         validation='Two clean stops with complete parent decisions/prefix/imply/boundary replay. Verify result trace hashes and selected snapshot framing. Compare clause identities and physical multiplicities, never counts alone. No performance or complete correctness claim.',
         test_consideration='Configuration-only reuse of frozen binary/formula observers and runner; syntax and full parent replay cover observer neutrality. Snapshot parsing receives positive/negative fixtures. No solver implementation changes.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
