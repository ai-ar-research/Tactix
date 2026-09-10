#!/usr/bin/env python3
"""Paired compact-trajectory successor after timeout and capped native detail."""
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
old = root/'benchmarks/runs/20260909-6757-rank21-search-window'
c = root/'benchmarks/runs/20260909-6774-rank21-light-window'
assert not c.exists()
classification = old/'classification-6773.json'
s = json.loads(classification.read_text())
assert s['candidate']['exit_code'] == 124
assert s['candidate']['available_decision_bytes_exact_prior_prefix']
assert s['native']['full50k_decisions_prefix_partial_proof_exact']
assert s['native']['detail_capped'] and s['native']['requested_window_rows'] == 0
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest, rel
m = json.loads((old/'manifest.json').read_text())
runner = (old/'run.py').read_text().replace('tactix-6757-', 'tactix-6774-')
anchor = "    prefix + '_EVENT_TRACE_CONFLICT_FROM': '25098',"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "    prefix + '_EVENT_TRACE_STOP_AFTER_CONFLICT': str(m['stop_conflict'] + 1),\n    prefix + '_EVENT_TRACE_TRAJECTORY_ONLY': '1',\n"+anchor)
compile(runner, str(c/'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c/folder).mkdir(parents=True)
for n in ('candidate', 'kissat', 'certificate_prefix.py'):
    shutil.copy2(old/'pins'/n, c/'pins'/n)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(c)).replace('6757', '6774'))
m['dependencies'][str(classification.relative_to(root))] = sha(classification)
for rel, digest in s['hashes'].items():
    m['dependencies'][rel] = digest
m.update(failed_observer_control=str(old.relative_to(root)),
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Complete same-pin Rank21 focused-window trajectory replay without discarded macro/heap snapshots or preprocessing Sweep records. Both6757 detail arms were unusable; this supplies missing native detail, not new timing.',
    hypothesis='Existing trajectory-only excludes Sweep noise and costly post-analysis ranking snapshots. Finite event-stop mode suppresses intermediate macro/heap snapshots; place it one conflict beyond the independent50K decision-prefix stop so it cannot replace that oracle.',
    validation='Require exact own6751 full50K decisions, prefix and partial proof, including native old10K proof prefix. Candidate macro may contain only tested source-backed additions; native boundaries only declared analysis/end additions. Reject empty/capped selected detail. No implications or full ranking snapshots claimed; learn/conflict trajectory localizes the next needed observation.',
    test_consideration='Configuration-only same-pin successor using existing trajectory and snapshot controls. Search-window/macro-replay and prefix/certificate fixtures plus explicit complete replay are the oracle. No production source/test change.',
    limitation='6757 candidate timeout, native preprocessing cap and6759 rejection preserved. No solver invalidity inferred, no solve-budget increase or policy change. Native diagnostic repeated only because selected detail was absent; fixed timing authority unchanged.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate', 'pins/kissat', 'pins/certificate_prefix.py', 'run.py', 'array.sbatch')}
with (c/'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2)
    stream.write('\n')
for p in (c/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
