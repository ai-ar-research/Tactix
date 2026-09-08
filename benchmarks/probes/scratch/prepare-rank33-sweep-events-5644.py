#!/usr/bin/env python3
"""Observe the unique missing equivalence without editing submitted trace pins."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5639-rank33-sweep-binaries'
c = root / 'benchmarks/runs/20260908-5644-rank33-sweep-events'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
prior = json.loads((old / 'summary.json').read_text())
assert prior['parent_full_replay']
assert prior['binary_comparisons'][1]['kissat_only'] == [
    {'item': [-143631, 143590], 'count': 1}, {'item': [-143590, 143631], 'count': 1}]
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-5639-', 'tactix-5644-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.sweep.tsv')),
    prefix + '_EVENT_TRACE_SWEEP_ONLY': '1',
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '120330',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '120330',
    prefix + '_EVENT_TRACE_LIMIT': '2000000'})
""")
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m = json.loads((old / 'manifest.json').read_text())
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(old.relative_to(root)), parent_summary_sha256=sha(old / 'summary.json'),
    hypothesis='At120330 first Sweep inputs agree; native alone adds the two directions of143590=143631, absent by the next Sweep input. Compare actual seed/environment/query/result traces before attributing it to scheduling, missing extraction or subsolver work.',
    validation='All parent traces, prefix and partial proof bytes replay exactly per arm through1M. Selected Sweep events are framed and untruncated; native summary events outside the selected window are counted separately, not evidence for this call.',
    test_consideration='Immutable observer configuration only; inherited parser checks and full parent replay plus dedicated window classification fixtures validate coverage. No solver unit-test change.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted Sweep result localization', job)
