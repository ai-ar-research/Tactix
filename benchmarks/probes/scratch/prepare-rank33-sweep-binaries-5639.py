#!/usr/bin/env python3
"""Resolve the remaining two-copy Sweep boundary without altering the solver."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5634-congruence-index-extended'
c = root / 'benchmarks/runs/20260908-5639-rank33-sweep-binaries'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
prior = json.loads((parent / 'summary.json').read_text())
assert prior['prior_common_prefix_replayed'] and prior['matching_rows'] == 18013384
boundary = json.loads((parent / 'boundary-localization.json').read_text())['first_coarse_boundary']
assert boundary['candidate']['name'] == 'Sweep'
assert boundary['candidate']['conflicts'] == boundary['kissat']['conflicts'] == 120330
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
runner = (parent / 'run.py').read_text().replace('tactix-5634-', 'tactix-5639-')
runner = runner.replace("selector = 'congruence_end' if arm == 'kissat' else 'name=Congruence|stage=end'",
                        "selector = 'sweep_' if arm == 'kissat' else 'name=Sweep|stage='")
assert runner.count("'9457'") == 2
runner = runner.replace("'9457'", "'120330'")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)))
m = json.loads((parent / 'manifest.json').read_text())
for key in ('predecessor', 'selected_authority', 'selection_authority', 'parent_summary_sha256', 'prior_summary_sha256'):
    m.pop(key, None)
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)), parent_summary_sha256=sha(parent / 'summary.json'),
    selected_authority=dict(path=str((parent / 'boundary-localization.json').relative_to(root)),
                            sha256=sha(parent / 'boundary-localization.json')),
    hypothesis='After cleanup all nonbinary portable boundaries agree before the182093 decision split. Sweep end120330 differs by two binary records. Compare complete before/after multisets and large-clause sets to distinguish duplicate retirement from unique-clause changes; do not infer cause from counts.',
    validation='Replay every parent decision, prefix, macro/boundary row and partial proof byte per arm exactly through1M. Validate complete before/after snapshot framing and multisets. No timing or checked final-verdict claim.',
    test_consideration='Configuration-only change using existing immutable observers. Full parent replay and framed snapshot checks supply observer-neutrality coverage; no solver unit-test change.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted Sweep binary localization', job)
