#!/usr/bin/env python3
"""Stage only explicit append-checkpoint evidence, never fuzz resources."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
paths=['PROGRESS.md']
helpers=['prepare-watch-pair-5466.py','check-watch-pair-5466.py',
 'build-watch-pair-5466.py','freeze-watch-pair-5466.py',
 'prepare-watch-pair-gates-5468.py','snapshot-watch-pair-runtime-5468.py',
 'seal-native-bound-5458.py','snapshot-eliminate-full-5454.py',
 'freeze-watch-checkpoint-5469.py','stage-watch-checkpoint-5469.py']
paths+=['benchmarks/probes/scratch/'+n for n in helpers]
paths+=['benchmarks/reports/'+n for n in ['2026-09-07-watch-pair-append.md',
 '2026-09-07-watch-pair-validation.json','2026-09-07-watch-pair-main-ctest.log']]
for number in (5466,5468,5469):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and
        p.suffix in ('.json','.py','.patch','.sbatch','.log') and p.stat().st_size<2000000]
    if (c/'matrix.tsv').exists():paths.append(str((c/'matrix.tsv').relative_to(root)))
for number in (5434,5435,5440,5441,5447,5448,5454,5455,5458,5462,5463,5464,5465):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results','provisional'):
        paths += [str(p.relative_to(root)) for p in (c/sub).glob('*.json')]
assert all((root/p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
