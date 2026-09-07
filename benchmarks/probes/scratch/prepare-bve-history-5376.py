#!/usr/bin/env python3
"""Track selected candidate eligibility history before the missing schedule member."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5375-bve-schedule'
new = root / 'benchmarks/runs/20260907-5376-bve-history'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for p in (old / 'pins').iterdir():
    shutil.copy2(p, new / 'pins' / p.name)
for p in old.glob('*.patch'):
    shutil.copy2(p, new / p.name)
source = (old / 'run.py').read_text()
source = source.replace('env["SAT_BVE_CANDIDATE_CONFLICT"] = "343785"',
                        'env["SAT_BVE_CANDIDATE_EXTERNAL"] = "10700"')
source = source.replace('env[prefix + "_QUEUE_STAMP_TRACE"] = str(C/"traces"/(arm+".queue.tsv"))\n', '')
(new / 'run.py').write_text(source)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old / 'manifest.json').read_text())
m.update(purpose='Locate preceding attempts for missing initial BVE member 10700.',
         hypothesis='Retry eligibility differs before initial scheduling at 343785; identify the last shared attempt before adding a marker observer.',
         acceptance='Both zero schedule stops, clean scratch, decision bytes equal 5375. Compare selected candidate begin/end history across all prior phases.',
         parent_campaign=str(old.relative_to(root)),
         source_note='Same immutable pins. Existing read-only selected candidate observer; schedule exits before the target BVE pass. No semantic change.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
