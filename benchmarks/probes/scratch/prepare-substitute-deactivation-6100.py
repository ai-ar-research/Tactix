#!/usr/bin/env python3
"""Reuse tested6090 observers,adding existing deactivation-order logs only."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-6092-rank37-macro-heaps'
c=root/'benchmarks/runs/20260908-6100-rank37-substitute-deactivation'
assert not c.exists()
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(8<<20),b''):h.update(b)
    return h.hexdigest()
s=json.loads((parent/'summary.json').read_text())
assert s['full_parent_replay'] and s['first_difference']['boundary']['key']==[272979,76952961,'Substitute_end',0]
m=json.loads((parent/'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(parent/rel)==digest,rel
for name in ('pins','logs','traces','results'):(c/name).mkdir(parents=True)
for arm in ('candidate','kissat'):
    shutil.copy2(parent/'pins'/arm,c/'pins'/arm);(c/'pins'/arm).chmod(0o555)
runner=(parent/'run.py').read_text().replace('tactix-6092-','tactix-6100-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
runner=runner.replace(anchor,"env[prefix+'_DEACT_TRACE']=str(c/'traces'/(arm+'.deact.log'))\n"+anchor)
compile(runner,str(c/'run.py'),'exec');(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)).replace('6092-rank37-macro-heaps','6100-rank37-deactivation'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),parent_campaign=str(parent.relative_to(root)),
         parent_summary_sha256=sha(parent/'summary.json'),
         hypothesis='First Substitute call starts with identical whole heap,ends296 positions different. Compare actual inheap deactivation sequence at272979,retaining nonmutating and repeated events;test order against common incoming heap before attributing a repair.',
         validation='Same immutable6090 binaries;all6092 ordinary1M,macro heaps,otherauxiliary traces andpartial proofbytes mustreplay beforeinterpreting deactivation history.',
         native_rerun_reason='Missing existing native deactivation-order log,not a timing refresh.',
         test_consideration='No newsolver orobservercode. Strict DEACT parser fixtures and fullparent replay required;Tactix-only trail metadata retained butnotusedasportablealignment.')
m['hashes']={name:sha(c/name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n');print(c)
