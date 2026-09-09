#!/usr/bin/env python3
"""Short added-rank adverse witness with both Tactix sources and native trace."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260909-6402-rank45-stamp-prefix'; assert not c.exists()
old=root/'benchmarks/runs/20260909-6376-rank35-stamp-prefix'
native=root/'benchmarks/runs/20260909-6303-rank32-later-prefix'
m=json.loads((old/'manifest.json').read_text()); nm=json.loads((native/'manifest.json').read_text())
for d,meta in ((old,m),(native,nm)):
    for rel,digest in meta['hashes'].items(): assert sha(d/rel)==digest,rel
selection=root/'benchmarks/reports/2026-09-09-compact-stamp-full50-6371/summary.json'
row=next(r for r in json.loads(selection.read_text())['rows'] if r['rank']==45)
assert row['parent_conflicts']==row['kissat_conflicts'] and row['parent_decisions']==row['kissat_decisions']
assert row['candidate_conflicts']>row['parent_conflicts'] and row['delta_parent_seconds']>27
full=json.loads((root/'benchmarks/runs/20260909-6366-compact-stamp-ranks1-50/manifest.json').read_text())
for name in ('pins','logs','results','traces'): (c/name).mkdir(parents=True)
for arm in ('candidate','previous'): shutil.copy2(old/'pins'/arm,c/'pins'/arm)
shutil.copy2(native/'pins/kissat',c/'pins/kissat')
runner=(old/'run.py').read_text().replace("('candidate', 'previous')","('candidate', 'previous', 'kissat')").replace('tactix-6376-','tactix-6402-')
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('--array=0-1','--array=0-2').replace('6376-rank35-stamp-prefix','6402-rank45-stamp-prefix'))
m.update(rank=45,input='benchmarks/cache/'+row['instance'],input_sha256_compressed=full['input_sha256'][row['instance']],
    stop_conflict=100000,stream_from=0,common_decisions=1,
    selection=dict(path=str(selection.relative_to(root)),sha256=sha(selection)),
    purpose='Locate first Rank45 divergence: parent6305 exactly matches native full conflict/decision counts,6357 regresses. Short added-rank UNSAT witness, not a solver parameter.',
    validation='Bind every pin and complete trace/prefix; verify row counts, first/last counters and cleanup, then compare all three pairs through100K. No inherited Rank45 trace identity is assumed.',
    test_consideration='Unchanged trace implementation and runner protocol; extend independent arm dispatch to3. Existing full-stream/prefix fixtures cover framing/truncation; manifest asserts exact arm coverage and selection rationale.',
    limitation='Stopped diagnostic, not timing/reference refresh or final-proof acceptance. No new source repair or observer-dependent causal claim from counters alone.')
m.pop('prior_closure'); m.pop('prior_common_hash')
m['authorities']['kissat']=dict(path=str((native/'manifest.json').relative_to(root)),sha256=sha(native/'manifest.json'))
m['resource_contract']['array']='0-2'
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
assert set(p.name for p in (c/'pins').iterdir())=={'candidate','previous','kissat'}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
