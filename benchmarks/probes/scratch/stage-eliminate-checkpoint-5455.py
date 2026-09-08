#!/usr/bin/env python3
"""Stage this checkpoint's explicit authorities, never fuzz-owned artifacts."""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[3]
paths=['PROGRESS.md','docs/scheduling-parity-ledger.md']
helpers=['prepare-eliminate-limit-5450.py','check-old-eliminate-limit-5450.py',
 'claude-eliminate-fixture-5450.txt','run-claude-eliminate-fixture-5450.py',
 'claude-eliminate-fixture-5450-response.json','claude-eliminate-fixture-5450-stderr.log',
 'check-old-eliminate-limit-5450-review.py','run-predecessor-unit-regression-5450-review.py',
 'build-eliminate-limit-5450.py','seal-eliminate-ledger-5449.py',
 'freeze-eliminate-limit-5450.py','prepare-eliminate-limit-gates-5453.py',
 'compare-eliminate-limit-prefix-5453.py','localize-eliminate-limit-boundary-5453.py',
 'seal-eliminate-limit-gates-5455.py','freeze-eliminate-checkpoint-5455.py',
 'stage-eliminate-checkpoint-5455.py']
paths+=['benchmarks/probes/scratch/'+name for name in helpers]
paths+=['benchmarks/reports/'+name for name in ['2026-09-07-eliminate-phase-budget.md',
 '2026-09-07-eliminate-budget-validation.json','2026-09-07-eliminate-budget-main-ctest.log']]
for number in (5450,5453,5454,5455):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and
        p.suffix in ('.json','.py','.patch','.sbatch','.log') and p.stat().st_size<2000000]
    if (c/'matrix.tsv').exists():paths.append(str((c/'matrix.tsv').relative_to(root)))
for number in (5423,5429,5434,5435,5437,5440,5441,5447,5448,5449):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    paths += [str(p.relative_to(root)) for p in (c/'results').glob('*.json')]
    paths += [str(p.relative_to(root)) for p in (c/'provisional').glob('*.json')]
for number,arms in [(5437,('previous','candidate','kissat')),(5449,('candidate',)),(5453,('candidate','kissat'))]:
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    for arm in arms:
        for kind in ('prefix','boundaries','macro'):
            p=c/'traces'/f'{arm}.{kind}.tsv'
            if p.exists() and p.stat().st_size<2000000:paths.append(str(p.relative_to(root)))
paths.append('benchmarks/runs/20260907-5449-rank37-eliminate-limit/logs/candidate.out')
assert all((root/p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
