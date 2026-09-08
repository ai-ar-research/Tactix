#!/usr/bin/env python3
"""Bracket the score update using immutable standalone macro snapshots."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5689-rank33-common-heap'
c = root / 'benchmarks/runs/20260908-5694-rank33-score-history'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
s = json.loads((parent / 'summary.json').read_text())
assert s['parent_full_replay'] and s['selected_score_relation'] == {'candidate':1,'kissat':-1}
assert s['differences']['score']['first_variables'] == [120425,120434,120438]
runner = (parent / 'run.py').read_text().replace('tactix-5689-', 'tactix-5694-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env.update({prefix + '_INPROCESS_SCORE_TRACE': str(c / 'traces' / (arm + '.scores.tsv')),
    'SAT_INPROCESS_SCORE_EXTERNAL': '120438', 'SAT_INPROCESS_SCORE_CONFLICT_FROM': '0'})
""" + anchor)
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'): (c / name).mkdir()
for arm in ('candidate','kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent),str(c)))
m = json.loads((parent / 'manifest.json').read_text())
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)), parent_summary_sha256=sha(parent / 'summary.json'),
    hypothesis='Only120425/120434/120438 have unequal scores at the last common decision, with approximately equal positive excess. Observe120438 at existing macro/slot boundaries from startup to bracket the score producer; distinguish a local update from global decay/rescaling before any repair.',
    validation='Full per-arm1M parent trace/heap/choice and partial-proof byte identity. Strict score framing and monotone counter validation; compare identically named/common counter boundaries by occurrence, retaining all unmatched rows. No global event trace, solver change or acceptance timing.',
    test_consideration='Immutable existing macro score observer; new parser positive/negative fixtures and complete replay validate this configuration use. No new solver unit tests needed until a state-transition repair is identified.',
    score_external=120438,score_compare_through=182093,
    limitation='A common boundary brackets a score difference, not the exact responsible update. Extra native/Tactix snapshot sites are retained and explicitly reported; no assumption that raw event sequences or active flags have identical scope.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [*(c / 'pins').iterdir(),c / 'run.py',c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
command = ['sbatch','--parsable',str((c / 'array.sbatch').relative_to(root))]
job = subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip()
assert job.isdecimal()
(c / 'submission.json').write_text(json.dumps(dict(job=job,command=command,
    manifest_sha256=sha(c / 'manifest.json')),indent=2)+'\n')
print('Submitted Rank33 score history',job)
