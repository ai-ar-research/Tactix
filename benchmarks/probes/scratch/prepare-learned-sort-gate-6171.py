#!/usr/bin/env python3
"""Prepare the unchanged B28+C5 gate with independently built new pins."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
authority = root / 'benchmarks/runs/20260909-6169-learned-sort-build'
a = json.loads((authority / 'manifest.json').read_text())
for rel, digest in a['binaries'].items():
    assert sha(root / rel) == digest, rel
old = root / 'benchmarks/runs/20260908-6114-substitute-publication-BC-gate'
c = root / 'benchmarks/runs/20260909-6171-learned-sort-BC-gate'
assert not c.exists()
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(old / rel) == digest, rel
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True)
for p in (old / 'pins').iterdir():
    if p.is_file() and not p.name.startswith('candidate'):
        shutil.copy2(p, c / 'pins' / p.name)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
for tier, number, kind in (('B', 6169, 'release'), ('C', 6170, 'bc')):
    binary = root / f'build/pin-{number}-learned-sort-{kind}/tactix'
    arm = 'candidate_' + tier
    shutil.copy2(binary, c / 'pins' / arm)
    m['arms'][arm].update(sha256=sha(binary), source_build=str(binary.parent.relative_to(root)),
        source_patch_sha256=sha(authority / 'cumulative.patch'))
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate.patch')
protected = json.loads((root / 'benchmarks/runs/20260909-6166-rank4-release-profile/manifest.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items():
    assert sha(root / rel) == digest, rel
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), predecessor=str(old.relative_to(root)),
    candidate_authority=str(authority.relative_to(root)), candidate_source=a['source'],
    protected_campaign_hashes=protected,
    purpose='Unchanged B28+C5 guard for private learned-level radix identity-pass shortcut.',
    hypothesis='Exact predecessor work/proof expected; test B/C regression before targeted Rank4 timing. No speedup assumed.',
    test_consideration=a['test_consideration'], patch_application='git apply --unidiff-zero')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in
    [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
