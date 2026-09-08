#!/usr/bin/env python3
"""Stage only full-bound scoring and explicitly scoped normal diagnostics."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
paths = ['PROGRESS.md']
helpers = ['analyze-bound-full-5477.py', 'prepare-rank37-extended-5478.py',
           'compare-rank37-extended-5478.py', 'prepare-rank32-prefix-5479.py',
           'compare-rank32-prefix-5479.py', 'freeze-bound-view-checkpoint-5480.py',
           'stage-bound-view-checkpoint-5480.py']
paths += ['benchmarks/probes/scratch/' + n for n in helpers]
paths += ['benchmarks/reports/' + n for n in
          ['2026-09-08-bound-full-and-next-losses.md',
           '2026-09-08-bound-full-validation.json',
           '2026-09-08-bound-full-main-ctest.log']]
for number in (5447,5448,5454,5455,5458,5462,5463,5464,5465,5468,5469,
               5470,5471,5475,5476,5478,5479):
    day = 20260908 if number >= 5472 else 20260907
    c = next((root / 'benchmarks/runs').glob(f'{day}-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results', 'provisional'):
        paths += [str(p.relative_to(root)) for p in (c / sub).glob('*.json')]
    if number in (5478,5479):
        paths += [str(p.relative_to(root)) for p in c.glob('*.py')]
        paths.append(str((c / 'array.sbatch').relative_to(root)))
assert all((root / p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
