#!/usr/bin/env python3
"""Observe the next Rank31 choice without changing its regressing source."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-5893-preclean-rank31-extended'
c=root/'benchmarks/runs/20260908-5910-preclean-rank31-choice'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((parent/'manifest.json').read_text())
s=json.loads((parent/'summary.json').read_text())
assert s['suffix_comparison']['left']==[394397,96579679,6205,487794,0]
for rel,digest in m['hashes'].items(): assert sha(parent/rel)==digest
c.mkdir(exist_ok=False)
for name in ('pins','logs','results','traces'): (c/name).mkdir()
for arm in ('candidate','kissat'): shutil.copy2(parent/'pins'/arm,c/'pins'/arm)
runner=(parent/'run.py').read_text().replace('tactix-5893-','tactix-5910-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
extra="""env.update(SAT_DECISION_HEAP_SNAPSHOT='1', SAT_EVENT_TRACE_HEAP_SET_AFTER_DECISION='1',
           SAT_EVENT_TRACE_HEAP_SET_PATH=str(c / 'traces' / (arm + '.heap.tsv')),
           SAT_EVENT_TRACE_HEAP_SET_CONFLICT='394397', SAT_EVENT_TRACE_HEAP_SET_DECISION='96579678',
           SAT_CHOICE_STATE_CONFLICT='394397', SAT_CHOICE_STATE_VARIABLE='87835')
env[prefix + '_CHOICE_STATE_TRACE'] = str(c / 'traces' / (arm + '.choice.tsv'))
"""
assert runner.count(anchor)==1
(c/'run.py').write_text(runner.replace(anchor,extra+anchor))
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('5893-preclean-rank31-extended','5910-preclean-rank31-choice'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),parent_campaign=str(parent.relative_to(root)),
    hypothesis='At first later Rank31 split394397/96579679 distinguish heap membership/score/pose from87835 assignment drift;test whether the independently fixed Reduce root-guidance lifecycle may own this later regression.',
    validation='Require every ordinary1M trace hash and partial proof byte to replay5893. Complete heap after last common choice96579678 and all87835 assignment/reason states at394397. No timing/promotion claim.',
    native_rerun_reason='Cached native5893 lacks full heap and assignment reasons at newly localized394397 choice;fixed runtime denominator unchanged.',
    cached_controls={arm:dict(record=str((parent/'results'/(arm+'.json')).relative_to(root)),sha256=sha(parent/'results'/(arm+'.json'))) for arm in ('candidate','kissat')},
    test_consideration='No source change. Existing standalone choice/heap observers,not broad tracing or a new stop selector;require complete ordinary replay before interpretation.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c/'run.py',c/'array.sbatch',c/'pins/candidate',c/'pins/kissat']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
