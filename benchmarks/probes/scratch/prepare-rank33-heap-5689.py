#!/usr/bin/env python3
"""Separate score order from physical heap pose at the last common decision."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5669-rank33-choice-state'
c = root / 'benchmarks/runs/20260908-5689-rank33-common-heap'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
s = json.loads((parent / 'summary.json').read_text())
assert s['parent_full_replay']
for arm in ('candidate', 'kissat'):
    assert s['selected_state'][arm]['stable'] == 1
    before = s['preceding_choices'][arm][-1]
    assert before['decisions'] == 49396660 and before['chosen'] == -123444
    assert before['value'] == 0
runner = (parent / 'run.py').read_text().replace('tactix-5669-', 'tactix-5689-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env.update({
    'SAT_DECISION_HEAP_SNAPSHOT': '1',
    'SAT_EVENT_TRACE_HEAP_SET_PATH': str(c / 'traces' / (arm + '.heap.tsv')),
    'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '182093',
    'SAT_EVENT_TRACE_HEAP_SET_DECISION': '49396660',
    'SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION': '1',
})
""" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)))
m = json.loads((parent / 'manifest.json').read_text())
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)), parent_summary_sha256=sha(parent / 'summary.json'),
    hypothesis='5669 excludes unavailable120438 and focused VMTF selection: stable score heap differs. At the last common decision compare all heap members, assignments, scores and physical positions; discriminate unequal relative score order from tie-pose before changing a mechanism.',
    validation='Exact full1M parent traces and partial-proof bytes per arm; strict heap schema, unique membership, contiguous positions and numeric max-heap invariant. No solver change or acceptance timing.',
    test_consideration='Reuse existing immutable one-shot heap observer; new positive/negative parser fixtures cover framing, values, membership, positions, nonfinite scores and heap violations. No solver unit-test change needed for configuration-only diagnostics.',
    snapshot_conflict=182093, snapshot_decisions=49396660,
    limitation='Post-decision snapshot precedes propagation of the last common decision. Active flags have different fixed/retired representations; proportional score differences alone do not prove ordering changes or a performance defect.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted Rank33 last-common heap', job)
