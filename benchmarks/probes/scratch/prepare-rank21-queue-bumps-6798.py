#!/usr/bin/env python3
"""Observe existing pre-move queue hook from the last inprocessing boundary."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable,str(root/'tools/slurm/session_deadline.py'),
    '--deadline','2026-09-10T06:00:00-04:00'],check=True)
old = root/'benchmarks/runs/20260909-6794-rank21-choice-state'
c = root/'benchmarks/runs/20260909-6798-rank21-queue-bumps'
assert not c.exists()
s = json.loads((old/'summary-6796.json').read_text())
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
contexts = {arm:{r['decisions']:r for r in s['arms'][arm]['contexts']} for arm in ('candidate','kissat')}
for arm in contexts:
    assert contexts[arm][25451309]['value'] == 0
    assert contexts[arm][25451309]['score'] == 6
assert contexts['candidate'][25451310]['value'] == 0
assert contexts['candidate'][25451309]['observed_next'] != contexts['kissat'][25451309]['observed_next']
m = json.loads((old/'manifest.json').read_text())
runner = (old/'run.py').read_text().replace('tactix-6794-','tactix-6798-')
anchor = '\ndef interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor,"\nenv[prefix + '_QUEUE_BUMP_TRACE'] = str(c/'traces'/(arm+'.bumps.tsv'))\nenv['SAT_QUEUE_BUMP_FROM'] = '23890'\nenv['SAT_QUEUE_BUMP_TO'] = '25102'\n"+anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for name in ('candidate','kissat','certificate_prefix.py'): shutil.copy2(old/'pins'/name,c/'pins'/name)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6794','6798'))
m['dependencies'].update(s['hashes'])
for p in (old/'summary-6796.json',root/'tools/bench/queue_bump_trace.py',root/'tools/bench/test_queue_bump_trace.py',
          root/'.claude/worktrees/runtime-reduce-watermark-6730/src/policies/heuristics.cpp',
          root/'tools/solvers/variants/kissat-macro-heap-6090/src/inlinequeue.h'):
    m['dependencies'][str(p.relative_to(root))] = sha(p)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    cached_controls={arm:str(old.relative_to(root)) for arm in ('candidate','kissat')},
    bump_window=[23890,25102],bump_row_cap=1000000,
    purpose='Locate the producer of Rank21 external402380 queue displacement, not an assignment skip or branch-selection hack.',
    hypothesis='Pre-move active-variable identity/order/value or links first diverge between the last23890 inprocessing boundary and the25102 split. Preserve absolute stamps; a uniform offset alone is not causal.',
    validation='Both exit0/clean; every6794 decision/prefix/boundary/macro/state trace and partial proof byte exactly replay, native old10K proof prefix exact. Only new11-column bumps.tsv allowed. Strict focused parser retains duplicate moves, raw links/stamps/values; no capped, empty, malformed or out-of-window trace.',
    test_consideration='No solver/observer source change. Existing five queue_bump_trace tests cover framing, duplicate/raw-state preservation, identity/value/window, monotonicity and caps; seven choice-state tests and complete6794 replay remain required.',
    limitation='Existing read-only scalar hooks only; no global event trace, additional stop, budget increase, source promotion, fuzz access or reference timing refresh.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as stream:
    json.dump(m,stream,indent=2); stream.write('\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
