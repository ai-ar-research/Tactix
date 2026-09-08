#!/usr/bin/env python3
"""Capture exact selected analyzed-bump admissions before the known choice split."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-5689-rank33-common-heap'
authority=root/'benchmarks/runs/20260908-5696-score-bump-observer'
c=root/'benchmarks/runs/20260908-5698-rank33-score-bumps'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
a=json.loads((authority/'manifest.json').read_text())
assert json.loads((parent/'summary.json').read_text())['parent_full_replay']
runner=(parent/'run.py').read_text().replace('tactix-5689-','tactix-5698-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
runner=runner.replace(anchor,"""env.update({prefix + '_SCORE_BUMP_TRACE': str(c / 'traces' / (arm + '.bumps.tsv')),
    'SAT_SCORE_BUMP_EXTERNAL': '120425,120434,120438',
    'SAT_SCORE_BUMP_CONFLICT_FROM': '0', 'SAT_SCORE_BUMP_CONFLICT_TO': '182093'})
"""+anchor)
compile(runner,str(c/'run.py'),'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm,binary in [('candidate','build/pin-5696-score-bump-observer-trace/tactix'),
    ('kissat','tools/solvers/variants/kissat-score-bump-observer-5696/build/kissat')]:
    assert sha(root/binary)==a['binaries'][binary]
    shutil.copy2(root/binary,c/'pins'/arm)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)))
m=json.loads((parent/'manifest.json').read_text())
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)),parent_summary_sha256=sha(parent/'summary.json'),
    candidate_authority=str(authority.relative_to(root)),candidate_source=a['sources']['tactix']['source'],
    observer_authority_sha256=sha(authority/'manifest.json'),source_patch_sha256=a['cumulative_patch_sha256'],
    hypothesis='The equal positive excess on three selected stable scores suggests one extra shared analyzed-variable bump. Compare all selected admissions, post-bump scores and current increments before182093; localize first admission/value difference before repairing analysis or its reason producer.',
    validation='Both clean1M stops,complete per-arm parent5689 trace/heap/choice and proof-byte identity,strict selected-bump parser,full ordered admission comparison and per-conflict multiset differences. No timing claim.',
    test_consideration=a['test_consideration'],score_external=[120425,120434,120438],score_window=[0,182093],
    limitation=a['limitations'])
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
command=['sbatch','--parsable',str((c/'array.sbatch').relative_to(root))]
job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip();assert job.isdecimal()
(c/'submission.json').write_text(json.dumps(dict(job=job,command=command,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
print('Submitted selected score bumps',job)
