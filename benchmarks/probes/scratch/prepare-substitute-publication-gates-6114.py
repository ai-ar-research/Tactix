#!/usr/bin/env python3
"""Prepare the unchanged B28+C5 gate and the approved full50 fixed view."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import fixed_runtime as f
authority=root/'benchmarks/runs/20260908-6107-substitute-publication'
a=json.loads((authority/'manifest.json').read_text())
release=root/'build/pin-6107-substitute-publication-release/tactix'
bc=root/'build/pin-6108-substitute-publication-bc/tactix'
for rel,digest in a['binaries'].items():assert f.sha(root/rel)==digest
old=root/'benchmarks/runs/20260908-6059-root-lock-BC-gate'
c=root/'benchmarks/runs/20260908-6114-substitute-publication-BC-gate'
m=json.loads((old/'manifest.json').read_text())
assert not c.exists()
for rel,digest in m['pin_sha256'].items():assert f.sha(old/rel)==digest,rel
for name in ('pins','logs','results','provisional','claims'):(c/name).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file() and not p.name.startswith('candidate'):shutil.copy2(p,c/'pins'/p.name)
shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6059-root-lock-BC','6114-substitute-publication-BC'))
for tier,binary in (('B',release),('C',bc)):
    arm='candidate_'+tier;shutil.copy2(binary,c/'pins'/arm)
    m['arms'][arm].update(sha256=f.sha(binary),source_build=str(binary.parent.relative_to(root)),
        source_patch_sha256=f.sha(authority/'cumulative.patch'))
shutil.copy2(authority/'cumulative.patch',c/'pins/candidate.patch')
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),predecessor=str(old.relative_to(root)),
    candidate_authority=str(authority.relative_to(root)),candidate_source=a['source'],
    purpose='Unchanged B28+C5 cumulative regression gate for general Substitute publication6107.',
    hypothesis='Native per-round lifecycle restores heap pose; require no B/C regression. No gain assumed.',
    test_consideration=a['test_consideration'],build_identity='IndependentRelease/BC hashes match.')
m['pin_sha256']={str(p.relative_to(c)):f.sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
print(c)
full=root/'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50'
f.prepare(full,authority,release,bc)
print(full)
