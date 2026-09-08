#!/usr/bin/env python3
"""Stage explicit bound-repair authorities, excluding fuzz-owned resources."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
paths=['DEVELOPMENT_PLAN.md','PROGRESS.md','docs/scheduling-parity-ledger.md']
helpers=['prepare-factor-observer-5456.py','build-factor-observer-5456.py',
 'freeze-factor-observer-5456.py','prepare-factor-window-5457.py',
 'compare-factor-window-5457.py','localize-factor-bound-5457.py',
 'prepare-native-bound-5458.py','prepare-bound-advance-5459.py',
 'check-old-bound-advance-5459.py','build-bound-advance-5459.py',
 'freeze-bound-advance-5459.py','prepare-bound-gates-5462.py',
 'finish-bound-full-5465.py','compare-bound-prefix-5463.py',
 'seal-bound-gates-5465.py','freeze-bound-checkpoint-5465.py',
 'stage-bound-checkpoint-5465.py']
paths+=['benchmarks/probes/scratch/'+n for n in helpers]
paths+=['benchmarks/reports/'+n for n in ['2026-09-07-bound-lifecycle.md',
 '2026-09-07-bound-lifecycle-validation.json','2026-09-07-bound-lifecycle-main-ctest.log']]
for number in (5456,5457,5458,5459,5462,5463,5464,5465):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and
        p.suffix in ('.json','.py','.patch','.sbatch','.log') and p.stat().st_size<2000000]
    if (c/'matrix.tsv').exists():paths.append(str((c/'matrix.tsv').relative_to(root)))
    for p in (c/'traces').glob('*.tsv'):
        if not p.name.endswith(('.decisions.tsv','.events.tsv')) and p.stat().st_size<2000000:
            paths.append(str(p.relative_to(root)))
for number in (5434,5435,5440,5441,5447,5448,5454,5455):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results','provisional'):
        paths += [str(p.relative_to(root)) for p in (c/sub).glob('*.json')]
assert all((root/p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:',len(set(paths)))
