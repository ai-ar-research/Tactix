#!/usr/bin/env python3
"""Extend the repaired Rank-34 decision frontier with unchanged trace pins."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root/'benchmarks/runs/20260907-5343-round-retirement'
runner = root/'benchmarks/runs/20260907-5328-bridge-decision-window'
new = root/'benchmarks/runs/20260907-5346-round-frontier'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'):
    (new/name).mkdir()
for p in (old/'pins').iterdir():
    shutil.copy2(p,new/'pins'/p.name)
for p in old.glob('*.patch'):
    shutil.copy2(p,new/p.name)
(new/'run.py').write_text((runner/'run.py').read_text().replace('"75451"','"200000"').replace('"75452"','"200001"').replace('"1000000"','"2000000"'))
(new/'array.sbatch').write_text((runner/'array.sbatch').read_text().replace(str(runner),str(new)))
m = json.loads((old/'manifest.json').read_text())
m.pop('job',None)
m.update(purpose='UNFUZZED bounded decision-frontier continuation after the complete warm-up closure in 5343.',
    hypothesis='The per-round retirement repair closes the known warm-up tie; the next decision divergence, if any before 200001, selects the next implementation owner.',
    acceptance='Both stop clean at 200001 without trace truncation. Compare decision counter/conflict/level/literal and distinguish warm-up from ordinary decisions; no timing or proof-verdict acceptance.',
    parent_campaign=str(old.relative_to(root)),
    source_note='Reuse exact immutable 5343 pins and source patches; broader selector only. Standalone deactivation logging remains unset.')
m['hashes'] = {str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [*(new/'pins').iterdir(),*new.glob('*.patch'),new/'run.py',new/'array.sbatch']}
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
