#!/usr/bin/env python3
"""Stage explicit checkpoint-owned evidence; retain large raw streams locally."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
paths = {root / 'PROGRESS.md'}
paths.update(root / 'benchmarks/reports' / name for name in (
    '2026-09-07-runtime-next-boundaries.md', '2026-09-07-runtime-boundaries-validation.json',
    '2026-09-07-runtime-boundaries-main-ctest.log'))
helpers = ('snapshot-rephase-full-5423.py', 'snapshot-rephase-profile-5425.py',
    'seal-rephase-replicas-5421.py', 'localize-rephase-boundary-5424.py',
    'prepare-rank37-prefix-5436.py', 'compare-rank37-prefix-5436.py',
    'prepare-sweep-binary-boundary-5437.py', 'compare-sweep-binary-boundary-5437.py',
    'freeze-runtime-boundaries-5437.py', 'stage-runtime-boundaries-5437.py')
paths.update(root / 'benchmarks/probes/scratch' / name for name in helpers)
for number in (5421, 5422, 5423, 5424, 5425, 5428, 5429, 5433, 5434, 5435, 5436, 5437):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    for kind in ('results', 'provisional'): paths.update((c / kind).glob('*.json'))
    paths.update(c.glob('*.json'))
    if number in (5436, 5437):
        paths.update(c / name for name in ('run.py', 'array.sbatch'))
    if number in (5424, 5433, 5436, 5437):
        for p in (c / 'traces').glob('*'):
            if p.stat().st_size <= 2_000_000 and 'decisions' not in p.name:
                paths.add(p)
    if number == 5425:
        paths.update((c / 'profiles').glob('*.txt'))
assert all(p.is_file() for p in paths)
subprocess.run(['git', 'add', '-f', '--', *[str(p.relative_to(root)) for p in sorted(paths)]], cwd=root, check=True)
print('Staged explicit checkpoint paths:', len(paths))
