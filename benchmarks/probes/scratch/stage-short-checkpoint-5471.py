#!/usr/bin/env python3
"""Stage explicit short-witness evidence, excluding all fuzz-owner resources."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
paths = ['PROGRESS.md']
helpers = ['seal-short-bound-witnesses-5462.py', 'prepare-square-prefix-5471.py',
           'compare-square-prefix-5471.py', 'localize-bound-boundary-5463.py',
           'freeze-short-checkpoint-5471.py', 'stage-short-checkpoint-5471.py']
paths += ['benchmarks/probes/scratch/' + n for n in helpers]
paths += ['benchmarks/reports/' + n for n in
          ['2026-09-08-short-witnesses-and-bound.md',
           '2026-09-08-short-bound-validation.json',
           '2026-09-08-short-bound-main-ctest.log']]
for number in (5434,5435,5440,5441,5447,5448,5454,5455,5458,5462,5463,
               5464,5465,5468,5469,5470,5471):
    c = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results', 'provisional'):
        paths += [str(p.relative_to(root)) for p in (c / sub).glob('*.json')]
    if number == 5471:
        paths += [str(p.relative_to(root)) for p in c.glob('*.py')]
        paths.append(str((c / 'array.sbatch').relative_to(root)))
    if number in (5458, 5463):
        paths += [str(p.relative_to(root)) for p in (c / 'traces').glob('*.tsv')
                  if p.stat().st_size < 2_000_000]
assert all((root / p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
