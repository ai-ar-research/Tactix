#!/usr/bin/env python3
"""Stop at the repaired incoming schedule; validate a nonstopping300K prefix."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6237-substitute-retry-rank32-prefix'
c=root/'benchmarks/runs/20260909-6248-repaired-rank32-membership'
assert not c.exists()
m=json.loads((old/'manifest.json').read_text()); s=json.loads((old/'summary.json').read_text())
assert s['prefixes']['candidate']['common_hash']=='10d4b4ca139060f5'
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
for name in ('pins','logs','traces','results'): (c/name).mkdir(parents=True)
shutil.copy2(old/'pins/candidate',c/'pins/candidate')
runner=(old/'run.py').read_text().replace('tactix-6237-','tactix-6248-')
oldflag="'SAT_DECISION_PREFIX_STOP': '1'"; assert runner.count(oldflag)==1
runner=runner.replace(oldflag,"'SAT_DECISION_PREFIX_STOP': '0'")
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor)==1
runner=runner.replace(anchor,"env['TACTIX_BVE_SCHEDULE_TRACE'] = str(c / 'traces/candidate.schedule.tsv')\nenv['SAT_BVE_SCHEDULE_CONFLICT'] = '386968'\nenv['SAT_BVE_SCHEDULE_ORDINAL'] = '0'\nenv['SAT_BVE_SCHEDULE_MEMBERS'] = '1'\nenv['SAT_BVE_SCHEDULE_SELECTED'] = '337674,390671,399915,409823,444046'\n"+anchor)
runner += "\nassert (c / 'traces/candidate.schedule.tsv').exists(), 'Required schedule stop absent'\n"
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6237-rank32','6248-rank32'))
m.update(stop_conflict=300000,actual_stop=dict(kind='initial BVE schedule',conflict=386968,ordinal=0),
    purpose='Repaired full incoming membership andselectedstate against cached native6208. Existing stopping observer is intentional,not a missing1M prefix.',
    validation='Require300K prefix common69828932/hash10d4b4ca139060f5, exact6237 decision/boundary prefix through386968 schedule entry, stopped schedule framing/membership/scores/occurrences, clean partial-proof deletion. Compare memberships before attributing newcandidate order.',
    parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')))
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print(c)
