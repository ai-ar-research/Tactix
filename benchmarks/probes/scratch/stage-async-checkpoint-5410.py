#!/usr/bin/env python3
"""Add only explicit checkpoint-owned evidence to the existing staged increment."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
paths = {root / 'PROGRESS.md', root / 'DEVELOPMENT_PLAN.md'}
paths.update(root / 'benchmarks/reports' / name for name in ('2026-09-07-async-check-warmup.md', '2026-09-07-async-check-validation.json'))
for n in range(5406, 5411):
    paths.update((root / 'benchmarks/probes/scratch').glob(f'*-{n}.py'))
    for c in (root / 'benchmarks/runs').glob(f'20260907-{n}-*'):
        for p in c.rglob('*'):
            if not p.is_file() or 'pins' in p.relative_to(c).parts or '__pycache__' in p.parts: continue
            if p.name.endswith(('.decisions.tsv', '.heap.tsv', '.choice.tsv', '.deact.tsv')): continue
            if p.stat().st_size > 5_000_000: continue
            if p.suffix in ('.json', '.py', '.sbatch', '.patch', '.tsv', '.log', '.out'): paths.add(p)
for n in (5383, 5389, 5393, 5395):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{n}-*'))
    paths.update((c / 'results').glob('*.json'))
    if (c / 'summary.json').exists(): paths.add(c / 'summary.json')
assert all(p.is_file() for p in paths)
subprocess.run(['git', 'add', '-f', '--', *[str(p.relative_to(root)) for p in sorted(paths)]], cwd=root, check=True)
print(f'Staged {len(paths)} explicit owned paths; no pins, large raw streams or active logs added')
