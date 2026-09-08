#!/usr/bin/env python3
"""Stage explicit full-view evidence and new causal probe, excluding fuzz."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
paths=['PROGRESS.md']
helpers=['analyze-eliminate-full-5470.py','prepare-rank31-prefix-5470.py',
 'compare-rank31-prefix-5470.py','freeze-full-view-checkpoint-5470.py',
 'stage-full-view-checkpoint-5470.py']
paths+=['benchmarks/probes/scratch/'+n for n in helpers]
paths+=['benchmarks/reports/'+n for n in ['2026-09-07-phase-budget-full-view.md',
 '2026-09-07-phase-budget-full-validation.json','2026-09-07-phase-budget-full-main-ctest.log']]
for number in (5434,5435,5440,5441,5447,5448,5454,5455,5458,5462,5463,5464,5465,5468,5469,5470):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results','provisional'):
        paths += [str(p.relative_to(root)) for p in (c/sub).glob('*.json')]
    if number==5470:
        paths += [str(p.relative_to(root)) for p in c.glob('*.py')]
        paths.append(str((c/'array.sbatch').relative_to(root)))
assert all((root/p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
