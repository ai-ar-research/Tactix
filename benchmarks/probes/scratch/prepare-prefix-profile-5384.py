#!/usr/bin/env python3
"""Pin a read-only profile of the already measured stopped-prefix executables."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5382-prefix-cost'
c = root / 'benchmarks/runs/20260907-5384-prefix-profile'
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'results', 'profiles'): (c / name).mkdir()
for name in ('tactix', 'kissat', 'runtime_task.py', 'candidate.patch'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(root / 'benchmarks/probes/scratch/prefix-profile-task-5384.py', c / 'pins/run.py')
script = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('bridge-decision-window', 'prefix-profile')
(c / 'array.sbatch').write_text(script)
m = json.loads((old / 'manifest.json').read_text())
m.update(purpose='Locate executable-level self and call-path hotspots behind the residual common-prefix instruction/time overhead.',
         hypothesis='Search propagation/analysis or inprocessing data handling account for the excess; profile before selecting a general representation repair.',
         parent_campaign=str(old.relative_to(root)),
         profile=dict(event='cycles:u', frequency=199, call_graph='dwarf,8192'),
         acceptance='Two clean intentional UNKNOWN stops with the same observed conflict/decision counts as 5382 and usable symbolized profiles. No verdict or timing acceptance.',
         limitations='Read-only sampling changes runtime. Self/inclusive samples locate hotspots, not a stable cost ratio. Prefix decisions agree through one million in 5381, not full state or the last two overshoot conflicts. No final proof acceptance or promotion.')
m['hashes'] = {str(p.relative_to(c)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(c / 'pins').iterdir(), c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
