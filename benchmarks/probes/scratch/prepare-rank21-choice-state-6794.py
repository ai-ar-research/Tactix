#!/usr/bin/env python3
"""Same-pin selected assignment/queue observation, with full original50K replay."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
old = root/'benchmarks/runs/20260909-6751-rank21-long-prefix'
c = root/'benchmarks/runs/20260909-6794-rank21-choice-state'
assert not c.exists()
comparison = root/'benchmarks/runs/20260909-6782-rank21-trajectory-stop/comparison-6793.json'
s = json.loads(comparison.read_text())
for rel,digest in s['hashes'].items():
    assert sha(root/rel) == digest
m = json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items():
    assert sha(old/rel) == digest
runner = (old/'run.py').read_text().replace('tactix-6751-', 'tactix-6794-')
marker = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(marker) == 1
runner = runner.replace(marker, marker+"\nenv.update({prefix + '_CHOICE_STATE_TRACE': str(c/'traces'/(arm+'.state.tsv')),\n    'SAT_CHOICE_STATE_CONFLICT': '25102', 'SAT_CHOICE_STATE_VARIABLE': '402380'})")
compile(runner, str(c/'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c/folder).mkdir(parents=True)
for name in ('candidate', 'kissat', 'certificate_prefix.py'):
    shutil.copy2(old/'pins'/name, c/'pins'/name)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6751','6794'))
m['dependencies'].update(s['hashes'])
m['dependencies'][str(comparison.relative_to(root))] = sha(comparison)
for p in (root/'tools/bench/choice_state.py', root/'tools/bench/test_choice_state.py'):
    m['dependencies'][str(p.relative_to(root))] = sha(p)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    cached_controls={arm:str(old.relative_to(root)) for arm in ('candidate','kissat')},
    observed_variable=402380, observed_conflict=25102,
    purpose='Distinguish assignment/implication from focused queue displacement at the validated Rank21 first differing choice.',
    hypothesis='Same selected external402380 aroundD25451309/310 will distinguish already-assigned native-first variable from a displaced queue/cursor. Do not assume the42-binary earlier residual caused this.',
    validation='Both exit0/clean and every original6751 decision/prefix/boundary/macro trace plus partial proof exactly match; native old10K proof prefix exact. Only additional18-column state file allowed, validated by choice_state parser and against every decision at conflict25102.',
    test_consideration='No solver/observer source change. Existing selected-variable hook records assignment/reason and focused queue columns. Seven new strict parser tests cover valid assigned/unassigned rows, framing, selection, numeric fields, reasons, ordering and arguments; full original50K replay is mandatory integration coverage.',
    limitation='No global event trace, additional endpoint, missing-prefix acceptance, solver-budget increase, native timing refresh, source promotion, fuzz access or FlatLists.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
