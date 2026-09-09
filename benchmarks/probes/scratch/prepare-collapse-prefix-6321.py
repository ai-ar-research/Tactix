#!/usr/bin/env python3
"""Replay current6305 against cached native6303; one new solver invocation."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6303-rank32-later-prefix'
c=root/'benchmarks/runs/20260909-6321-collapse-rank32-prefix'
a=root/'benchmarks/runs/20260909-6305-substitute-collapse-build/manifest.json'
authority=json.loads(a.read_text())
binary=root/'build/pin-6305-substitute-collapse-trace/tactix'
assert sha(binary)==authority['binaries'][str(binary.relative_to(root))]
for rel,digest in authority['files'].items():assert sha(root/authority['source']/rel)==digest,rel
native=json.loads((old/'results/kissat.json').read_text())
assert native['exit_code']==0 and native['scratch_deleted']
assert sha(old/'pins/kissat')==native['binary_sha256']
assert not c.exists()
for name in ('pins','traces','results','logs'):(c/name).mkdir(parents=True)
shutil.copy2(binary,c/'pins/candidate')
(c/'run.py').write_text((old/'run.py').read_text().replace('tactix-6303-','tactix-6321-'))
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6303-rank32-later','6321-collapse-prefix').replace('--array=0-1\n','--array=0\n')
assert '--array=0\n' in batch and '--array=0-1' not in batch
(c/'array.sbatch').write_text(batch)
m=json.loads((old/'manifest.json').read_text())
m.update(candidate_authority={'path':str(a.relative_to(root)),'sha256':sha(a)},
    cached_native={'path':str((old/'results/kissat.json').relative_to(root)),'sha256':sha(old/'results/kissat.json')},
    scope='Current6305 trace; native6303 reused without a new Kissat invocation.',
    purpose='Test whether current Substitute collapse repair preserves the1M closure and changes the later Rank32 split.',
    hypothesis='Retained endpoint retry can change later BVE scheduling; no assumed causal or performance benefit.',
    validation='Hash/frame-check both traces and inherited1M prefix; compare every later row through3M. Earlier-prefix change invalidates this localization, not solver correctness.',
    test_consideration='No runner or observer code changes beyond scratch label; reuse decision-prefix and boundary parser regression fixtures. Current6305 unit/proof gates bound by source authority.',
    limitation='Cached native is a deterministic trace oracle, not matched timing. Partial proofs are hash-bound and deleted, not accepted final certificates.')
m['resource_contract']['array']='0'
m['hashes']={name:sha(c/name) for name in ('pins/candidate','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
(c/'pins/candidate').chmod((c/'pins/candidate').stat().st_mode & ~0o222)
print(c)
