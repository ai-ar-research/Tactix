#!/usr/bin/env python3
"""Extend identical current/previous traces close to the earlier full solve."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from runtime_summary import counters
old=root/'benchmarks/runs/20260909-6459-rank35-later-prefix'
c=root/'benchmarks/runs/20260909-6467-rank35-near-final-prefix'; assert not c.exists()
prior=old/'summary.json'; s=json.loads(prior.read_text())
assert s['decisions']['complete_equal'] and s['matching_decisions_from_start']==159573175
assert s['prefixes']['candidate']['hash']==s['prefixes']['previous']['hash']=='4bfd01ec4333149f'
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
full=root/'benchmarks/runs/20260909-6320-substitute-collapse-ranks1-50/results/034.json'
record=json.loads(full.read_text()); total=counters(record)['conflicts']
assert total==4434673
# Last100K diagnostic grid point below the earlier full solve; not a policy cap.
stop=total//100000*100000; assert stop==4400000 and stop<total
for folder in ('pins','logs','traces','results'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir(): shutil.copy2(p,c/'pins'/p.name)
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6459-','tactix-6467-'))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6459-rank35-later','6467-rank35-near-final'))
m.update(stop_conflict=stop,stream_from=3000000,common_decisions=159573175,
    prior_common_hash='4bfd01ec4333149f',
    prior_closure={'path':str(prior.relative_to(root)),'sha256':sha(prior)},
    earlier_closure={str(prior.relative_to(root)):sha(prior)},
    prior_full_solve={'path':str(full.relative_to(root)),'sha256':sha(full),'conflicts':total},
    purpose='Localize Rank35 current6424/6305 split after159573175 matching decisions through3M. Stop at last100K diagnostic grid point below previous full4434673-conflict solve.',
    validation='Both inherited3M common hashes and complete threshold overlap must match before comparing every suffix row to4.4M. Earlier-prefix change requires localization, not inherited closure. Own partial-proof hash/deletion is not final acceptance.',
    test_consideration='Same immutable pins/runner and maintained65benchmark13watcher plus complete-stream/overlap malformed-row controls. New assertions bind exact previous fullconflicts and safe diagnostic stop. No solver, observer, checker or parser edit.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
