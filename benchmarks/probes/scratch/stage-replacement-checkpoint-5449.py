#!/usr/bin/env python3
"""Stage only explicitly scoped parent-owned evidence, never fuzz artifacts."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
paths = ['PROGRESS.md']
helpers = ['annotate-inprocessing-5443.py','annotate-inprocessing-5444.py',
    'prepare-replacement-cursor-5445.py','check-old-replacement-cursor-5445.py',
    'build-replacement-cursor-5445.py','freeze-replacement-cursor-5445.py',
    'prepare-replacement-cursor-gates-5447.py','snapshot-replacement-cursor-runtime-5447.py',
    'seal-replacement-cursor-gate-5448.py','snapshot-inprocess-full-5434.py',
    'freeze-replacement-checkpoint-5448.py','localize-inprocess-boundary-5436.py',
    'prepare-rank37-eliminate-limit-5449.py','audit-late-rank37-5449.py',
    'stage-replacement-checkpoint-5449.py']
paths += ['benchmarks/probes/scratch/'+name for name in helpers]
paths += ['benchmarks/reports/'+name for name in ['2026-09-07-replacement-cursor.md',
    '2026-09-07-replacement-cursor-validation.json','2026-09-07-replacement-cursor-main-ctest.log',
    '2026-09-07-rank37-late-validation.json']]
for number in (5443,5444,5445,5447,5448,5449):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and
        p.suffix in ('.json','.py','.patch','.tsv','.sbatch','.log','.txt','.stderr')]
for number in (5425,5428,5434,5436):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    paths += [str(p.relative_to(root)) for p in (c/'results').glob('*.json')]
    paths += [str(p.relative_to(root)) for p in (c/'provisional').glob('*.json')]
    if number==5436:
        for name in ('candidate.boundaries.tsv','kissat.boundaries.tsv','candidate.macro.tsv',
                     'candidate.prefix.tsv','kissat.prefix.tsv'):
            paths.append(str((c/'traces'/name).relative_to(root)))
assert all((root/path).is_file() for path in paths)
subprocess.run(['git','add','-f','--',*paths],cwd=root,check=True)
print('Staged explicit parent-owned paths:',len(paths))
