#!/usr/bin/env python3
"""Capture the next selected Rank-37 Factor seed/grid and budget boundary."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5453-rank37-eliminate-limit-prefix'
c=root/'benchmarks/runs/20260907-5457-rank37-factor-window'
authority=root/'benchmarks/runs/20260907-5456-factor-observer'
assert (authority/'manifest.json').exists()
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm,binary in [('candidate','build/pin-5456-factor-observer-trace/tactix'),
                   ('kissat','tools/solvers/variants/kissat-factor-observer-5456/build/kissat')]:
    shutil.copy2(root/binary,c/'pins'/arm)
runner=(old/'run.py').read_text().replace("'tactix-5453-'","'tactix-5457-'")
needle="env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '0'"
assert runner.count(needle)==1
runner=runner.replace(needle,"""env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '32073'
env['SAT_INPROCESS_BOUNDARY_CONFLICT_FROM'] = '32073'
env[prefix + '_EVENT_TRACE'] = str(c/'traces'/(arm+'.events.tsv'))
env[prefix + '_EVENT_TRACE_CONFLICT_FROM'] = '32073'
env[prefix + '_EVENT_TRACE_CONFLICT_TO'] = '32073'
env[prefix + '_EVENT_TRACE_LIMIT'] = '2000000'
env[prefix + '_EVENT_TRACE_FACTOR_SCHEDULE'] = '1'
env['SAT_EVENT_TRACE_FACTOR_ONLY'] = '1'
if arm == 'candidate': env['TACTIX_SLOT_TRACE'] = '1'
""")
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('bridge-decision-window','5457-factor-window'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((old/'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),candidate_authority=str(authority.relative_to(root)),
 candidate_source='.claude/worktrees/runtime-factor-observer-5456',stop_conflict=32074,stream_from=0,
 created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
 hypothesis='The second Factor call at conflict 32073 has common coarse input but introduces 3 versus 8 extensions. Compare selected seed order, accepted grids and native ticks against the existing Tactix slot ledger to distinguish ordering/material/effort causes.',
 validation='Two controlled proof-on stops, no trace truncation, exact each-arm prefix against corresponding 5453 full stream through 32073. Compare portable Factor payloads; no acceptance timing or complete correctness claim.',
 contract='Full-width array 0-1, one solver per 12-CPU/48000-MiB task, no throttle/node constraint; partial certificates hashed and scratch deleted.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'pins/candidate',c/'pins/kissat',c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(0o555)
print(c)
