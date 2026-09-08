#!/usr/bin/env python3
"""Distinguish assignment availability from VMTF order at the first choice split."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5650-propagation-cursor-rank33-prefix'
selected = root / 'benchmarks/runs/20260908-5634-congruence-index-extended/summary.json'
c = root / 'benchmarks/runs/20260908-5669-rank33-choice-state'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
assert json.loads((parent / 'summary.json').read_text())['parent_full_replay']
first = json.loads(selected.read_text())['first_decision_difference']
assert first['candidate'] == ['182093', '49396661', '7257', '-120438', '0']
assert first['kissat'] == ['182093', '49396661', '7257', '-121103', '0']
runner = (parent / 'run.py').read_text().replace('tactix-5650-', 'tactix-5669-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env.update({prefix + '_CHOICE_STATE_TRACE': str(c / 'traces' / (arm + '.state.tsv')),
    'SAT_CHOICE_STATE_CONFLICT': '182093', 'SAT_CHOICE_STATE_VARIABLE': '120438'})
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
    selected_authority=dict(path=str(selected.relative_to(root)), sha256=sha(selected)),
    hypothesis='The first variable choice split at182093 may be unequal assignment availability or unequal focused VMTF order/cursor,not the earlier root-satisfied Sweep equivalence. Observe120438 at every choice in that conflict before selecting an implementation repair.',
    validation='Both clean1M stops and complete per-arm parent trace/proof-byte identity;all18-column selected state rows must match the decision stream. No broad active event trace,solver change,acceptance timing or complete-verdict claim.',
    test_consideration='Configuration-only reuse of immutable tested choice observer. New strict state parser positive/negative fixtures plus complete parent trace/proof identity validate this new use; no solver unit-test change needed.',
    state_columns='conflicts decisions level chosen observed value assignment_level heap_position score reason stable queue_stamp search_external search_stamp observed_stamp observed_prev observed_next chosen_stamp'.split(),
    selected_conflict=182093, selected_variable=120438, selected_decision=49396661)
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
command = ['sbatch', '--parsable', str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job, command=command,
    manifest_sha256=sha(c / 'manifest.json')), indent=2) + '\n')
print('Submitted selected Rank33 choice state', job)
