#!/usr/bin/env python3
"""Checked six-task diagnostic of the cumulative, unfuzzed bridge-pose candidate."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5318-core-unit-small'
new = root / 'benchmarks/runs/20260907-5329-bridge-unit-small'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (new / name).mkdir()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for name in ('shipped', 'checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'):
    shutil.copy2(old / 'pins' / name, new / 'pins' / name)
shutil.copy2(root / 'build/pin-5327-bridge-pose/tactix', new / 'pins/candidate')
shutil.copy2(root / 'benchmarks/runs/20260907-5327-bridge-root-order/candidate.patch', new / 'pins/candidate.patch')
assert sha(new / 'pins/candidate') == 'a8868f435ddef9a4c6d65c3b852ffe3ac4fa015dc23e3e96187927d1cb197b36'
assert sha(new / 'pins/shipped') == 'de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f'
for name in ('matrix.tsv', 'aggregate.py'):
    shutil.copy2(old / name, new / name)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
manifest = json.loads((old / 'manifest.json').read_text())
manifest.pop('slurm_job_id', None)
manifest.update(
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    hypothesis='Core exposure plus sequential rewrite and bridge/physical pose reduces downstream Rank-34 work without losing the Rank-17 gain or imposing material matched-counter Rank-21 overhead.',
    comparison='Fuzz-passing c2 r2 versus unfuzzed cumulative 5327 Release; unchanged Ranks 17/21/34 matrix; proof-on both.',
    decision='All six models must check and scratch be clean. One diagnostic wave chooses further investigation; no acceptance/promotion or decision-grade speedup claim.',
    parent_campaign=str(old.relative_to(root)),
    candidate_validation='77/77 CTests, including duplicate/absent bridge pose and 44 DRAT/LRAT proof cases; no fuzz acceptance yet.')
manifest['arms']['candidate'].update(sha256=sha(new / 'pins/candidate'), source_patch_sha256=sha(new / 'pins/candidate.patch'))
manifest['pin_sha256'] = {str(p.relative_to(new)): sha(p) for p in (new / 'pins').iterdir()}
for name in ('matrix.tsv', 'array.sbatch'):
    manifest['pin_sha256'][name] = sha(new / name)
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
