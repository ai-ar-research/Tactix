#!/usr/bin/env python3
"""Stage only checkpoint-owned immutable evidence, not active logs or binaries."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
paths={root/'PROGRESS.md'}
paths.update(root/'benchmarks/reports'/name for name in ('2026-09-07-root-entry-followup.md','2026-09-07-root-entry-validation.json','2026-09-07-root-entry-main-ctest.log'))
for number in range(5425,5436):
    paths.update((root/'benchmarks/probes/scratch').glob(f'*-{number}.py'))
    for c in (root/'benchmarks/runs').glob(f'20260907-{number}-*'):
        for p in c.rglob('*'):
            if not p.is_file() or '__pycache__' in p.parts:continue
            rel=p.relative_to(c)
            if 'pins' in rel.parts:
                if p.suffix in ('.py','.patch'):paths.add(p)
                continue
            if number not in (5426,5430) and ('logs' in rel.parts or 'profiles' in rel.parts or 'traces' in rel.parts):continue
            if p.stat().st_size>5_000_000:continue
            if p.suffix in ('.json','.py','.sbatch','.patch','.tsv','.log','.out','.asm'):paths.add(p)
for number in (5395,5421,5422,5423):
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    for kind in ('results','provisional'):paths.update((c/kind).glob('*.json'))
    if (c/'summary.json').exists():paths.add(c/'summary.json')
assert all(p.is_file() for p in paths)
subprocess.run(['git','add','-f','--',*[str(p.relative_to(root)) for p in sorted(paths)]],cwd=root,check=True)
print('Staged explicit owned paths:',len(paths))
