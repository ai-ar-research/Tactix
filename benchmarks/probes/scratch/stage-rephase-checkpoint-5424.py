#!/usr/bin/env python3
"""Stage the exact isolated repair and checkpoint evidence, never live pins."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
paths = {root / 'PROGRESS.md'}
paths.update(root / 'benchmarks/reports' / name for name in (
    '2026-09-07-rephase-root-closure.md', '2026-09-07-rephase-root-validation.json',
    '2026-09-07-rephase-root-main-ctest.log'))
for number in range(5411, 5425):
    paths.update((root / 'benchmarks/probes/scratch').glob(f'*-{number}.py'))
    for c in (root / 'benchmarks/runs').glob(f'20260907-{number}-*'):
        for p in c.rglob('*'):
            if not p.is_file() or '__pycache__' in p.parts: continue
            rel = p.relative_to(c)
            if 'pins' in rel.parts:
                if p.suffix in ('.py', '.patch'): paths.add(p)
                continue
            if number >= 5421 and 'logs' in rel.parts: continue
            if p.name.endswith(('.decisions.tsv', '.heap.tsv', '.choice.tsv', '.deact.tsv')): continue
            if p.stat().st_size > 5_000_000: continue
            if p.suffix in ('.json', '.py', '.sbatch', '.patch', '.tsv', '.log', '.out'): paths.add(p)
old = root / 'benchmarks/runs/20260907-5395-eliminate-entry-ranks1-40'
paths.update((old / 'results').glob('*.json'))
if (old / 'summary.json').exists(): paths.add(old / 'summary.json')
assert all(p.is_file() for p in paths)
subprocess.run(['git', 'add', '-f', '--', *[str(p.relative_to(root)) for p in sorted(paths)]], cwd=root, check=True)
print(f'Staged {len(paths)} explicit owned paths; binary pins, large streams and active logs remain local')
