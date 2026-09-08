#!/usr/bin/env python3
"""Stage complete append timing evidence and explicitly scoped normal results."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
paths = ['PROGRESS.md', 'benchmarks/probes/scratch/freeze-watch-measurement-5481.py',
         'benchmarks/probes/scratch/stage-watch-measurement-5481.py']
paths += ['benchmarks/reports/' + n for n in
          ['2026-09-08-watch-pair-measured.md',
           '2026-09-08-watch-pair-measured-validation.json',
           '2026-09-08-watch-pair-measured-main-ctest.log']]
for number in (5447,5448,5454,5455,5458,5462,5463,5464,5465,5468,5469,
               5470,5471,5475,5476,5478,5479):
    day = 20260908 if number >= 5472 else 20260907
    c = next((root / 'benchmarks/runs').glob(f'{day}-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results', 'provisional'):
        paths += [str(p.relative_to(root)) for p in (c / sub).glob('*.json')]
assert all((root / p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
