#!/usr/bin/env python3
"""Prepare isolated core-metadata observer; no unit-emission behavior changes."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5313-core-boundary-selector'
new = root / 'benchmarks/runs/20260907-5316-core-capture-observer'
new.mkdir(exist_ok=False)
for name in ('pins', 'traces', 'logs'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5316-core-capture/tactix', new / 'pins/tactix')
shutil.copy2(old / 'pins/kissat', new / 'pins/kissat')
for name in ('run.py', 'array.sbatch'):
    content = (old / name).read_text().replace(str(old), str(new)).replace(
        '5313-core-boundary-selector', '5316-core-capture-observer')
    (new / name).write_text(content)
for path in (new / 'pins').iterdir():
    path.chmod(path.stat().st_mode & ~0o222)
manifest = dict(
    purpose='Observe true recorded core antecedents independently of native-LRAT chains; no host consequence or search-policy change.',
    source_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    source_patch='candidate.patch',
    reference_campaign=str(old.relative_to(root)),
    input_sha256_compressed='ae23202ebf6de47a653fb74430a7603e155a321f2826929864e9bfb69c41ebbd',
    contract='Full-width 0-1, 12 logical CPUs and 48000 MiB/task, one bound solver, no throttle/node constraint; 600-second diagnostic cap.',
    acceptance='Semantic event traces match 5313; compare saved core rows at exact seed/query; scratch deleted. Stopped runs are not verdict or timing gates.',
    implementation='Reuses metadata portions of 9a4adfa..d42c71d, omitting search changes; root-unit antecedents are shallow in physical clause order, separately from the LRAT cone.',
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (new / 'pins').iterdir()})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(new)
