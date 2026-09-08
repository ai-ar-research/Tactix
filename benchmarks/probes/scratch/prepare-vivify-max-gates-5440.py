#!/usr/bin/env python3
"""Freeze exact-work timing pairs and the unchanged cumulative B28+C5 gate."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
authority = runs / '20260907-5438-vivify-max'
assert (authority / 'manifest.json').exists()
builds = {t: root / p / 'tactix' for t, p in [('B', 'build/pin-5438-vivify-max-release'), ('C', 'build/pin-5439-vivify-max-bc')]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(builds['B']) == sha(builds['C'])
def init(c):
    c.mkdir(exist_ok=False)
    for d in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / d).mkdir()
def finish(c, m):
    m.pop('slurm_job_id', None)
    m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        candidate_authority=str(authority.relative_to(root)))
    m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
    (c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
    for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
    print(c)
old = runs / '20260907-5428-watch-truncate-rank35-runtime'
c = runs / '20260907-5440-vivify-max-rank35-runtime'
init(c)
m = json.loads((old / 'manifest.json').read_text())
for arm, binary, patch in [('candidate', builds['B'], authority / 'cumulative.patch'),
    ('previous', root / 'build/pin-5430-inprocess-root-release/tactix', runs / '20260907-5430-inprocess-root/cumulative.patch')]:
    shutil.copy2(binary, c / 'pins' / arm)
    shutil.copy2(patch, c / 'pins' / (arm + '.patch'))
    m['arms'][arm].update(sha256=sha(binary), source_build=str(binary.parent.relative_to(root)),
        source_patch='pins/' + arm + '.patch', source_patch_sha256=sha(patch))
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'): shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m.update(parent_campaign=str(old.relative_to(root)),
    candidate_validation='144 exact old/new schedule/pose cases; independent identical Release builds each pass 79/79 fixed tests; UNFUZZED.',
    hypothesis='Linear maximum selection avoids unnecessary irredundant literal sorting while preserving exact work and proof bytes on the clean profile witness.',
    decision='Require identical complete non-timing statistics and proof hashes in all three pairs against phase-entry 5430. Judge median paired ratio and full range, not best-of; absolute headline only with both arm ranges <=2%. Pending proofs guide investigation but not final acceptance. Neutral/noisy timing does not establish a useful optimization.')
finish(c, m)
old = runs / '20260907-5435-inprocess-root-BC-gate'
c = runs / '20260907-5441-vivify-max-BC-gate'
init(c)
m = json.loads((old / 'manifest.json').read_text())
for tier in ('B', 'C'):
    for role in ('current', 'candidate'):
        arm = role + '_' + tier
        binary = builds[tier] if role == 'candidate' else old / 'pins' / arm
        shutil.copy2(binary, c / 'pins' / arm)
        m['arms'][arm]['sha256'] = sha(binary)
        if role == 'candidate': m['arms'][arm].update(source_build=str(binary.parent.relative_to(root)), source_patch_sha256=sha(authority / 'cumulative.patch'))
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'): shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('5435-inprocess-root-BC', '5441-vivify-max-BC'))
m.update(predecessor=str(old.relative_to(root)), purpose='Unchanged cumulative B28+C5 gate for linear maximum-count Vivify ranking against production.',
    hypothesis='Preserve expected verdicts and cumulative regression behavior with the exact-rank scheduling optimization.',
    build_identity='Independent immutable Release 5438/5439, matching SHA-256, 79/79 tests each. Event trace/conflict stop/journal/shadow disabled.')
finish(c, m)
