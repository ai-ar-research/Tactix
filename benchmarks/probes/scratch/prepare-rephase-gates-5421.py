#!/usr/bin/env python3
"""Pin three matched Rank-35 replicate blocks and the unchanged B28+C5 gate."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260907-5417-rephase-root'
assert (authority / 'manifest.json').exists()
builds = {tier: root / path / 'tactix' for tier, path in [('B', 'build/pin-5419-rephase-root-release'), ('C', 'build/pin-5420-rephase-root-bc')]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(builds['B']) == sha(builds['C'])
def init(c):
    c.mkdir(exist_ok=False)
    for d in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / d).mkdir()
def finish(c, m):
    m.pop('slurm_job_id', None)
    m['created_at'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)
old = runs / '20260907-5392-eliminate-entry-runtime'
c = runs / '20260907-5421-rephase-root-rank35-runtime'
init(c)
m = json.loads((old / 'manifest.json').read_text())
arms = {}
for name, before, binary in [('candidate', 'candidate', builds['B']), ('previous', 'candidate', old / 'pins/candidate'), ('kissat', 'kissat', old / 'pins/kissat')]:
    shutil.copy2(binary, c / 'pins' / name)
    arm = dict(m['arms'][before])
    arm.update(path='pins/' + name, sha256=sha(binary))
    if name != 'kissat':
        patch = authority / 'cumulative.patch' if name == 'candidate' else old / 'pins/candidate.patch'
        shutil.copy2(patch, c / 'pins' / (name + '.patch'))
        arm.update(source_patch='pins/' + name + '.patch', source_patch_sha256=sha(patch))
    arms[name] = arm
m['arms'] = arms
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'): shutil.copy2(old / 'pins' / name, c / 'pins' / name)
with (runs / '20260907-5395-eliminate-entry-ranks1-40/matrix.tsv').open(newline='') as f:
    row = next(r for r in csv.DictReader(f, delimiter='\t') if r['rank'] == '35')
with (c / 'matrix.tsv').open('w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=['task', 'rank', 'arm', 'tier', 'instance', 'expected'], delimiter='\t')
    w.writeheader()
    for wave in range(3):
        for offset, arm in enumerate(arms): w.writerow(dict(row, task=3*wave+offset, arm=arm))
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-11', '--array=0-8'))
input_authority = json.loads((runs / '20260907-5395-eliminate-entry-ranks1-40/manifest.json').read_text())
m['input_sha256'] = {row['instance']: input_authority['input_sha256'][row['instance']]}
m.update(parent_campaign=str(old.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
         candidate_validation='5417 old-negative/new-positive root-closure fixture, 76/76 trace and 77/77 in each independent Release build; UNFUZZED.',
         hypothesis='Restoring the missing root propagation before Rephase reduces Rank-35 search work and time relative to the exact preceding coupled candidate.',
         comparison='Three independent matched replicate blocks, candidate/previous/pristine Kissat; instance-major arm order, all tasks eligible concurrently.',
         wave_tasks={str(w): list(range(3*w, 3*w+3)) for w in range(3)},
         decision='Use the median of all three paired candidate/previous ratios and full range. Absolute runtime headline requires each arm range <=2%. Check all nine proofs and cleanup; expected pending results guide continued work provisionally. No promotion or global claim; UNFUZZED.')
m['resource_contract']['array'] = '0-8'
finish(c, m)
old = runs / '20260907-5393-eliminate-entry-BC-gate'
c = runs / '20260907-5422-rephase-root-BC-gate'
init(c)
m = json.loads((old / 'manifest.json').read_text())
for tier in ('B', 'C'):
    for name in ('current', 'candidate'):
        arm = name + '_' + tier
        binary = builds[tier] if name == 'candidate' else old / 'pins' / arm
        shutil.copy2(binary, c / 'pins' / arm)
        m['arms'][arm]['sha256'] = sha(binary)
        if name == 'candidate':
            m['arms'][arm].update(source_build=str(binary.parent.relative_to(root)), source_patch_sha256=sha(authority / 'cumulative.patch'))
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'): shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('5380-vivify-retry-BC-gate', '5422-rephase-root-BC'))
m.update(predecessor=str(old.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
         purpose='Complete unchanged checked B28+C5 gate for the cumulative root-closure candidate against production.',
         hypothesis='Restore reference root closure before phase changes without solve loss or unacceptable cumulative regression.',
         build_identity='Independent Release 5419 and 5420 from immutable 5417 source, identical hashes; 77/77 fixed tests in each. Trace/conflict-stop/journal/shadow disabled.',
         continuation='Checks run asynchronously; expected solver results are provisional passes for dependent investigation, not checked-gate closure.')
finish(c, m)
