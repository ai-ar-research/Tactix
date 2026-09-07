#!/usr/bin/env python3
"""Pin the pose-only bridge experiment; prior campaign artifacts are immutable."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5326-composed-root-order'
new = root / 'benchmarks/runs/20260907-5327-bridge-root-order'
source = root / '.claude/worktrees/runtime-bridge-pose-5327'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
shutil.copy2(root / 'build/pin-5327-bridge-trace/tactix', new / 'pins/tactix')
shutil.copy2(old / 'pins/kissat', new / 'pins/kissat')
patch = []
for folder in ('src', 'tests'):
    for path in sorted((source / folder).rglob('*')):
        if not path.is_file():
            continue
        rel = path.relative_to(source).as_posix()
        original = subprocess.check_output(['git', 'show', 'c2a95a6:' + rel], cwd=root).decode()
        candidate = path.read_text()
        if original != candidate:
            patch.extend(difflib.unified_diff(original.splitlines(True), candidate.splitlines(True),
                                             fromfile='a/' + rel, tofile='b/' + rel))
(new / 'candidate.patch').write_text(''.join(patch))
for name in ('tactix-observer.patch', 'kissat-observer.patch', 'run.py', 'compare.py'):
    shutil.copy2(old / name, new / name)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('composed-root-order', 'bridge-root-order'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract', 'kissat_source', 'acceptance')}
manifest.update(
    purpose='UNFUZZED pose-only ablation on 5326: move last extant copies of freshly emitted bridges at their per-pair rewrite boundary. Known pairs are not moved. All proofs still precede all rewrites.',
    limitation='Physical copies have no independent identity: an earlier rewrite can change a future bridge. This is an ordering ablation, not a proven general birth-order reconstruction.',
    release_sha256=hashlib.sha256((root / 'build/pin-5327-bridge-pose/tactix').read_bytes()).hexdigest(),
    focused_validation='test_sweep, test_substitute (duplicate/absent-image pose fixture), test_subsolver, and 44 DRAT/LRAT proof cases pass.',
    reference_campaign=str(old.relative_to(root)),
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch',
                      new / 'tactix-observer.patch', new / 'kissat-observer.patch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
