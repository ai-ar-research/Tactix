#!/usr/bin/env python3
"""Determine whether the second Factor pass starts with unequal heap membership."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260908-5571-rank32-factor-heap'
c=root/'benchmarks/runs/20260908-5580-rank32-factor-entry'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''):h.update(b)
    return h.hexdigest()
prior=json.loads((old/'heap-localization.json').read_text())
assert prior['parent_replay'] and prior['candidate_only']==[-491572,119877,393688]
m=json.loads((old/'manifest.json').read_text())
for name,digest in m['hashes'].items():assert sha(old/name)==digest
runner=(old/'run.py').read_text().replace('tactix-5571-','tactix-5580-')
assert runner.count("'SAT_EVENT_TRACE_FACTOR_HEAP_SET_STAGE': 'updated'")==1
runner=runner.replace("'SAT_EVENT_TRACE_FACTOR_HEAP_SET_STAGE': 'updated'",
                      "'SAT_EVENT_TRACE_FACTOR_HEAP_SET_STAGE': 'initial'")
compile(runner,str(c/'run.py'),'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm in ('candidate','kissat'):shutil.copy2(old/'pins'/arm,c/'pins'/arm)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),factor_ordinal=4693,
    selected_authority=dict(path=str((old/'heap-localization.json').relative_to(root)),sha256=sha(old/'heap-localization.json')),
    hypothesis='After4824 matched grids, heap membership differs with shared scores equal. Compare the second pass initial heap after4693 prior grids to distinguish admission/marks from within-pass updates and nonproductive pops.',
    validation='Two clean100K stops and full per-arm5537 replay/proof hash; standalone heap observer at initial stage and factor ordinal4693. Complete heap membership, scores, physical positions and invariants. No solver policy change or timing claim.',
    test_consideration='No observer or solver code change. Reuse8 heap parser fixtures plus exact whole parent replay and immutable pin/input hashes; explicit stage/ordinal binding distinguishes same-conflict passes.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
print(c)
