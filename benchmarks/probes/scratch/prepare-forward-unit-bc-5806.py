#!/usr/bin/env python3
"""Create a separate immutable B28+C5 gate; never edit either running campaign."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
old = runs / '20260908-5754-reuse-target-BC-gate'
authority = runs / '20260908-5798-forward-unit'
campaign = runs / '20260908-5806-forward-unit-BC-gate'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
a = json.loads((authority / 'manifest.json').read_text())
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in a['files'].items(): assert sha(root / a['source'] / rel) == digest
for rel, digest in a['binaries'].items(): assert sha(root / rel) == digest
for rel, digest in m['pin_sha256'].items(): assert sha(old / rel) == digest
assert not (campaign / 'manifest.json').exists() and not (campaign / 'array.sbatch').exists()
campaign.mkdir(exist_ok=True)  # may resume this helper's failed-before-launch copy
for folder in ('pins','logs','results','provisional','claims'):
    (campaign / folder).mkdir(exist_ok=True)
    if folder != 'pins': assert not list((campaign / folder).iterdir())
for p in (campaign / 'pins').iterdir():
    assert p.is_file() and ('pins/' + p.name) in m['pin_sha256']
    assert sha(p) == sha(old / 'pins' / p.name)
for p in (old / 'pins').iterdir():
    if not p.is_file() or p.name.startswith('candidate'): continue
    if (campaign / 'pins' / p.name).exists(): continue  # verified identical above
    shutil.copy2(p, campaign / 'pins' / p.name)
shutil.copy2(old / 'matrix.tsv', campaign / 'matrix.tsv')
batch = (old / 'array.sbatch').read_text().replace(str(old), str(campaign))
batch = batch.replace('--job-name=5530-sweep-small-BC', '--job-name=5806-forward-unit-BC')
(campaign / 'array.sbatch').write_text(batch)
for tier, number, mode in [('B',5798,'release'),('C',5799,'bc')]:
    build = root / f'build/pin-{number}-forward-unit-{mode}'
    arm = 'candidate_' + tier
    shutil.copy2(build / 'tactix', campaign / 'pins' / arm)
    m['arms'][arm].update(sha256=sha(campaign / 'pins' / arm),
        source_build=str(build.relative_to(root)), source_patch_sha256=sha(authority / 'cumulative.patch'))
assert m['arms']['candidate_B']['sha256'] == m['arms']['candidate_C']['sha256']
shutil.copy2(authority / 'cumulative.patch', campaign / 'pins/candidate.patch')
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    predecessor=str(old.relative_to(root)), candidate_authority=str(authority.relative_to(root)),
    candidate_source=a['source'], test_consideration=a['test_consideration'],
    purpose='B28+C5 cumulative regression gate for isolated canonical forward-unit continuation5798.',
    hypothesis='Continuing productive forward rounds preserves expected verdicts and cumulative B28+C5 runtime relative to production; score solved counts, paired times, PAR-2 and memory without claiming Tier-D success.',
    build_identity=a['test_consideration'],
    status='UNFUZZED investigation material; expected solve outcomes permit provisional continuation while proof checks run. No promotion.')
m['pin_sha256'] = {str(p.relative_to(campaign)):sha(p) for p in
                  [*(campaign / 'pins').iterdir(), campaign / 'array.sbatch', campaign / 'matrix.tsv']}
(campaign / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (campaign / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(campaign)
