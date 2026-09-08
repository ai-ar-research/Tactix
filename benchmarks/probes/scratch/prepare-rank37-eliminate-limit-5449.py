#!/usr/bin/env python3
"""Measure the existing elimination ledger at the first extra forward round."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5436-rank37-inprocess-prefix'
c = root / 'benchmarks/runs/20260907-5449-rank37-eliminate-limit'
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'): (c/name).mkdir()
shutil.copy2(old/'pins/candidate', c/'pins/candidate')
runner = (old/'run.py').read_text().replace("'tactix-5436-'", "'tactix-5449-'")
runner = runner.replace("env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '0'", """env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '29761'
env['SAT_INPROCESS_BOUNDARY_CONFLICT_FROM'] = '29761'
env['TACTIX_SLOT_TRACE'] = '1'
env['TACTIX_BVE_CANDIDATE_TRACE'] = str(c/'traces/candidate.bve.tsv')
env['SAT_BVE_CANDIDATE_CONFLICT'] = '29761'""")
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c))
    .replace('--array=0-1','--array=0').replace('bridge-decision-window','5449-eliminate-limit'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old/'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), stop_conflict=29762, stream_from=29761,
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    hypothesis='At conflict 29761 candidate and native agree through BVE end; candidate alone performs another Strengthen round. Test whether the existing resolution ledger is already over its phase ceiling, as native eliminate.c checks before another forward round.',
    validation='One clean proof-on controlled stop; pins, selected BVE ticks/limit rows, slot ledger and normalized boundary counts. No solver change, acceptance timing, or claim that extra simplification is intrinsically weaker.',
    contract='Single full-width task 0, one bound proof-on solver, 12 logical CPUs/48000 MiB, no throttle or node constraint. Native boundary comparison is already frozen in 5436.')
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [c/'pins/candidate',c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(c/'pins/candidate').chmod(0o555)
print(c)
