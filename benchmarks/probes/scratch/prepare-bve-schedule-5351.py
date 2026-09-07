#!/usr/bin/env python3
"""Freeze the initial elimination heap before the first candidate at 95799."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5350-bve-frontier'
new = root / 'benchmarks/runs/20260907-5351-bve-schedule'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for p in (old / 'pins').iterdir():
    shutil.copy2(p, new / 'pins' / p.name)
for p in old.glob('*.patch'):
    shutil.copy2(p, new / p.name)
source = (old / 'run.py').read_text()
start = source.index('env[prefix + "_BVE_CANDIDATE_TRACE"]')
end = source.index('try:\n', start)
source = source[:start] + '''env[prefix + "_BVE_SCHEDULE_TRACE"] = str(C/"traces"/(arm+".schedule.tsv"))
env["SAT_BVE_SCHEDULE_CONFLICT"] = "95799"
env["SAT_BVE_SCHEDULE_ORDINAL"] = "0"
env["SAT_BVE_SCHEDULE_MEMBERS"] = "1"
''' + source[end:]
(new / 'run.py').write_text(source)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
m = json.loads((old / 'manifest.json').read_text())
m.pop('job', None)
m.update(purpose='UNFUZZED initial BVE schedule comparison before any candidate at conflict 95799.',
         hypothesis='The tenth candidate mismatch may originate in the initial heap or in updates during the first nine candidates.',
         acceptance='Both exit zero at the first selected schedule, before candidate execution, with clean scratch; compare complete membership, scores, counts and physical positions.',
         parent_campaign=str(old.relative_to(root)),
         source_note='Reuse exact immutable 5348 pins; existing standalone schedule observer stops before first candidate. No verdict/timing claim.')
m['hashes'] = {str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in [*(new / 'pins').iterdir(), *new.glob('*.patch'), new / 'run.py', new / 'array.sbatch']}
(new / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
