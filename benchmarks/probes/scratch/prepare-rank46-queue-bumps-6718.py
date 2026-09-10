#!/usr/bin/env python3
"""Observe pre-move queue links/stamps at the earliest Rank46 branch split."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6714-rank46-first-swap'
c=root/'benchmarks/runs/20260909-6718-rank46-queue-bumps'; assert not c.exists()
s=json.loads((old/'summary-6716.json').read_text()); assert s['observer_replay_validated']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
for event in ('imply','learned','analysis_begin','analysis_end'):
    assert s['raw_event_comparison'][event]['first_raw_difference'] is None,event
m=json.loads((old/'manifest.json').read_text())
runner=(old/'run.py').read_text().replace('tactix-6714-','tactix-6718-')
anchor='\ndef interrupted(signum, frame):'; assert runner.count(anchor)==1
runner=runner.replace(anchor,"""
env[prefix + '_QUEUE_BUMP_TRACE'] = str(c/'traces'/(arm+'.bumps.tsv'))
env['SAT_QUEUE_BUMP_FROM'] = '78790'
env['SAT_QUEUE_BUMP_TO'] = '78798'
env[prefix + '_CHOICE_STATE_TRACE'] = str(c/'traces'/(arm+'.choice.tsv'))
env['SAT_CHOICE_STATE_CONFLICT'] = '78797'
env['SAT_CHOICE_STATE_VARIABLE'] = '18626'
"""+anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file(): shutil.copy2(p,c/'pins'/p.name)
for arm in ('candidate','kissat'):
    p=old/f'results/{arm}.json'; r=json.loads(p.read_text()); m['dependencies'][str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items(): m['dependencies'][str((old/'traces'/name).relative_to(root))]=digest
for p in (old/'summary-6716.json',root/'tools/bench/queue_bump_trace.py',root/'tools/bench/test_queue_bump_trace.py'):
    m['dependencies'][str(p.relative_to(root))]=sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6714-rank49-prefix','6718-rank46-queue-bumps'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),cached_controls={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    bump_window=[78790,78798],bump_row_cap=100000,choice_variable=18626,choice_conflict=78797,
    purpose='6716 preserves all7214 implications/eight learned clauses in78790..78798. First decisionswap at78797 has inactive random choice and same negative phases. Record pre-move queue links/stamps and18626 choice states to distinguish bump order, earlier relative stamps and cursor state. No causal link to later watch ordering assumed.',
    validation='Require byte-exact all6714 global and standalone traces,1M decisions/prefix/partial proof and prior100K proof prefix. Only new bumps/choice traces allowed; preserve raw queue stamps and values, not only variable sets.',
    test_consideration='Five new fixed parser tests cover duplicate preservation, malformed/truncated rows, focused identity, values, window, monotonicity and caps. Existing exact-replay tests still apply; no solver code changed or duplicate fuzz needed.')
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as f: json.dump(m,f,indent=2); f.write('\n')
for p in (c/'pins').iterdir():
    if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
