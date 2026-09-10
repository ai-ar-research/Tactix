#!/usr/bin/env python3
"""Retain one reachable prefix stop and disable expensive unrelated observers."""
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
old = root/'benchmarks/runs/20260909-6774-rank21-light-window'
c = root/'benchmarks/runs/20260909-6782-rank21-trajectory-stop'
assert not c.exists()
classification = old/'classification-6781.json'
s = json.loads(classification.read_text())
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest, rel
m = json.loads((old/'manifest.json').read_text())
runner = (old/'run.py').read_text().replace('tactix-6774-', 'tactix-6782-')
changes = {"str(m['stop_conflict'] + 1)": "str((1 << 64) - 2)",
    "prefix + '_EVENT_TRACE_SEARCH_ONLY': '1', ": ''}
for before, after in changes.items():
    assert runner.count(before) == 1, before
    runner = runner.replace(before, after)
compile(runner, str(c/'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c/folder).mkdir(parents=True)
for name in ('candidate', 'kissat', 'certificate_prefix.py'):
    shutil.copy2(old/'pins'/name, c/'pins'/name)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(c)).replace('6774','6782'))
m['dependencies'].update(s['hashes'])
m['dependencies'][str(classification.relative_to(root))] = sha(classification)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Repair the classified competing-stop configuration while retaining full50K replay and the focused25102 choice investigation on unchanged6730/native6090 pins.',
    hypothesis='UINT64_MAX is the disabled-event-stop sentinel; UINT64_MAX-1 selects existing finite-mode snapshot suppression without a reachable event stop in this bounded run. Prefix stop alone owns termination. TRAJECTORY_ONLY alone supplies the requested event classes; remove the unnecessary broad SEARCH_ONLY admission.',
    validation='Both exit0 with exact own6751 full50K streams/prefixes/partial proofs and native old10K proof prefix. Candidate macro additions and native analysis-end boundary additions remain explicitly normalized; no extra50001 endpoint allowed in selected detail and no capped/empty trace.',
    test_consideration='No source edit. Existing prefix, search-window, macro-replay and truncation-marker fixtures plus complete original50K replay are the integration oracle. The6781 failure demonstrates why requested+1 is not a safe second stop.',
    limitation='Preserve6757 and6774 failures and strict6759/6776. No native timing refresh, solve-budget increase, missing-prefix acceptance, semantic change or source promotion.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2)
    stream.write('\n')
for p in (c/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
