#!/usr/bin/env python3
"""Extend the complete three-arm Rank33 closure through one million conflicts."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6470-rank33-activation-prefix'
c=root/'benchmarks/runs/20260909-6486-rank33-later-prefix'; assert not c.exists()
prior=old/'summary.json'; s=json.loads(prior.read_text())
assert s['rank']==33 and s['stop']==100000
assert all(p['complete_equal'] and p['matching_rows']==31494584 for p in s['pairs'].values())
assert all(p['count']==31494584 and p['hash']=='5c263e00aa882466' for p in s['prefixes'].values())
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest
m=json.loads((old/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(old/rel)==digest
for folder in ('pins','logs','traces','results'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir(): shutil.copy2(p,c/'pins'/p.name)
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6470-','tactix-6486-'))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6470-rank33-activation-prefix','6486-rank33-later-prefix'))
m.update(stop_conflict=1000000,stream_from=100000,common_decisions=31494584,
    prior_common_hash='5c263e00aa882466',
    prior_closure={'path':str(prior.relative_to(root)),'sha256':sha(prior)},
    parent_runs={arm:str(old.relative_to(root)) for arm in ('candidate','previous','kissat')},
    purpose='Current6424, parent6383 and native match all31494584 Rank33 decisions through100K. Extend all three through1M without presuming continued equality.',
    validation='Validate all own common-prefix hashes, complete100K overlap and cumulative row/counter accounting, then compare every suffix row in all three pairs. No timing reference refresh or final-proof acceptance.',
    test_consideration='Same immutable pins and three-arm runner. Maintained complete-row, overlap and derived-counter tests apply; new validator binds all three cached prefixes and their actual threshold overlaps. No solver/observer/parser change.',
    limitation='Bounded first decision localization or equality only, not cause, full formula/solve identity, final proof or runtime acceptance.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
