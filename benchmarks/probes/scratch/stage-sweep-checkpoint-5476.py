#!/usr/bin/env python3
"""Stage explicit Sweep repair evidence and normal gates, never fuzz resources."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
paths = ['PROGRESS.md']
helpers = ['annotate-sweep-rewrite-5472.py', 'prepare-sweep-cursor-5473.py',
           'check-old-sweep-cursor-5473.py', 'build-sweep-cursor-5473.py',
           'freeze-sweep-cursor-5473.py', 'prepare-sweep-cursor-gates-5475.py',
           'snapshot-sweep-cursor-runtime-5475.py', 'seal-sweep-cursor-gate-5476.py',
           'snapshot-bound-full-5465.py', 'freeze-sweep-checkpoint-5476.py',
           'stage-sweep-checkpoint-5476.py']
paths += ['benchmarks/probes/scratch/' + n for n in helpers]
paths += ['benchmarks/reports/' + n for n in
          ['2026-09-08-sweep-membership-cursor.md',
           '2026-09-08-sweep-cursor-validation.json',
           '2026-09-08-sweep-cursor-main-ctest.log']]
for number in (5472,5473,5475,5476):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and
              p.suffix in ('.json','.py','.patch','.sbatch','.log','.txt','.stderr')
              and p.stat().st_size < 2_000_000]
    if (c / 'matrix.tsv').exists(): paths.append(str((c / 'matrix.tsv').relative_to(root)))
for number in (5447,5448,5454,5455,5458,5462,5463,5464,5465,5468,5469,5470,5471):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results','provisional'):
        paths += [str(p.relative_to(root)) for p in (c / sub).glob('*.json')]
assert all((root / p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
