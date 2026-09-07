#!/usr/bin/env python3
"""List/stage only owned small evidence; retain binaries/large streams locally."""
import collections
from pathlib import Path
import re
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
paths=[root/p for p in ['PROGRESS.md','docs/calculus.md','docs/implementation-notes.md',
 'docs/scheduling-parity-ledger.md','benchmarks/reports/2026-09-07-congruence-prefix-frontier.md',
 'benchmarks/reports/2026-09-07-congruence-prefix-artifact-index.json']]
scratch=root/'benchmarks/probes/scratch'
for p in scratch.iterdir():
 if re.search(r'-(536\d|537[012])\.(py|c|cpp)$',p.name): paths.append(p)
for n in range(5360,5373):
 c=next((root/'benchmarks/runs').glob('20260907-'+str(n)+'-*'))
 for p in c.iterdir():
  if p.is_file() and p.suffix in ('.py','.sbatch','.patch','.json','.log','.tsv'): paths.append(p)
 for p in (c/'pins').glob('*'):
  if p.suffix in ('.py','.patch'): paths.append(p)
 for p in (c/'traces').glob('*'):
  if any(p.name.endswith('.'+s+'.tsv') for s in ('state','prefix','counts','boundaries')): paths.append(p)
 for p in (c/'results').glob('*.json'): paths.append(p)
 for p in (c/'logs').glob('*.log'):
  task=re.search(r'_(\d+)\.log$',p.name)
  if n<5371 or (task and (c/'results'/f'{int(task[1]):03d}.json').exists()): paths.append(p)
 if n<5371:
  paths.extend((c/'logs').glob('*.out'))
old=root/'benchmarks/runs/20260907-5357-empty-factor-BC-gate'
paths.extend([old/'summary.json',old/'results/064.json',old/'results/065.json'])
paths.extend((old/'logs').glob('*64*'))
paths.extend((old/'logs').glob('*65*'))
paths=sorted(set(p for p in paths if p.is_file()))
assert all(p.stat().st_size<5_000_000 for p in paths)
print('Owned files:',len(paths),'bytes:',sum(p.stat().st_size for p in paths))
groups=collections.Counter(str(p.relative_to(root)).split('/')[0] for p in paths)
print(dict(groups))
if '--stage' in sys.argv:
 subprocess.run(['git','add','-f','--',*[str(p.relative_to(root)) for p in paths]],cwd=root,check=True)
else:
 for p in paths: print(p.relative_to(root))
