#!/usr/bin/env python3
"""Extend the proven6269/native one-million-conflict closure, not timing."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6276-vivify-binary-rank32-prefix'
native=root/'benchmarks/runs/20260909-6183-rank32-current-prefix'
s=json.loads((old/'summary.json').read_text())
assert s['matching_decisions_from_start']==96083379 and s['decisions']['complete_equal']
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
c=root/'benchmarks/runs/20260909-6303-rank32-later-prefix'
assert not c.exists()
for name in ('pins','traces','results','logs'):(c/name).mkdir(parents=True)
shutil.copy2(old/'pins/candidate',c/'pins/candidate')
shutil.copy2(native/'pins/kissat',c/'pins/kissat')
assert sha(c/'pins/kissat')=='a902fccbe9e63c2eedc1aa4332a7d912d0859088aaa98740b90facc6373f1add'
code=(old/'run.py').read_text().replace('tactix-6276-','tactix-6303-')
(c/'run.py').write_text(code)
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6276-rank32-prefix','6303-rank32-later').replace('--array=0\n','--array=0-1\n')
assert '--array=0-1' in batch
(c/'array.sbatch').write_text(batch)
m=json.loads((old/'manifest.json').read_text())
m.update(stop_conflict=3000000,stream_from=1000000,common_decisions=96083379,
    parent_summary=dict(path=str((old/'summary.json').relative_to(root)),sha256=sha(old/'summary.json')),
    parent_runs=dict(candidate=str(old.relative_to(root)),kissat=str(native.relative_to(root))),
    purpose='Locate next Rank32 divergence after6269 matches all96083379 decisions through1M but worsens full-run work.',
    hypothesis='A later portable effect or scheduling mismatch remains; locate its first decision boundary before another repair.',
    validation='Require inherited96083379-rowhash33974c0cb2f02138,all threshold-overlap rows,full earlier per-arm boundary/macro prefixes,then every later decision through3M. No full-formula or timing claim.',
    test_consideration='No solver/observer source change; reuse tested prefix/boundary readers with strict framing and byte-prefix checks. New pins,two independent12CPU48000MiB/1200solver tasks; partial certificates hash/delete.')
m['hashes']={name:sha(c/name) for name in ('pins/candidate','pins/kissat','run.py','array.sbatch')}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
