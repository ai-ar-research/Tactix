#!/usr/bin/env python3
"""Stage owned measurement/causal evidence only, excluding all fuzz artifacts."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
paths = ['PROGRESS.md']
paths += ['benchmarks/probes/scratch/' + n for n in (
    'prepare-sweep-full-5485.py', 'snapshot-sweep-full-5485.py', 'seal-sweep-full-5485.py',
    'validate-square-phase-5486.py', 'prepare-square-imply-5487.py', 'compare-square-imply-5487.py',
    'localize-rank32-boundary-5488.py', 'localize-rank37-extended-5489.py',
    'prepare-rank32-sweep-5490.py', 'compare-rank32-sweep-5490.py',
    'localize-rank37-nonbinary-5491.py', 'claude-periodic-fuzz-5492.txt',
    'run-claude-periodic-fuzz-5492.py', 'freeze-sweep-causal-5493.py',
    'freeze-sweep-causal-final-5494.py', 'freeze-sweep-causal-controller-5495.py',
    'stage-sweep-causal-5493.py')]
paths += ['benchmarks/reports/' + n for n in (
    '2026-09-08-sweep-measured-and-causal-followup.md',
    '2026-09-08-sweep-causal-validation.json', '2026-09-08-sweep-causal-main-ctest.log',
    '2026-09-08-sweep-causal-final-main-ctest.log')]
for number in (5447,5448,5454,5455,5458,5462,5463,5464,5465,5468,5469,
               5470,5471,5475,5476,5478,5479,5482,5485,5487,5490):
    day = 20260908 if number >= 5472 else 20260907
    c = next((root / 'benchmarks/runs').glob(f'{day}-{number}-*'))
    paths += [str(p.relative_to(root)) for p in c.glob('*.json')]
    for sub in ('results', 'provisional'):
        paths += [str(p.relative_to(root)) for p in (c / sub).glob('*.json')]
    if number in (5485,5487,5490):
        for name in ('array.sbatch','matrix.tsv','run.py'):
            if (c / name).exists():
                paths.append(str((c / name).relative_to(root)))
        paths += [str(p.relative_to(root)) for p in (c / 'pins').glob('*.py')]
        paths += [str(p.relative_to(root)) for p in (c / 'pins').glob('*.patch')]
assert all((root / p).is_file() for p in paths)
subprocess.run(['git','add','-f','--',*sorted(set(paths))],cwd=root,check=True)
print('Staged explicit checkpoint paths:', len(set(paths)))
