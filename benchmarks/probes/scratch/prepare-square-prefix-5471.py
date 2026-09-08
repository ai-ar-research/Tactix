#!/usr/bin/env python3
"""Use a checked short B witness to localize an inprocessing discrepancy."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3];runs=root/'benchmarks/runs'
old=runs/'20260907-5463-rank37-bound-prefix';c=runs/'20260907-5471-square-bound-prefix'
checked=json.loads((runs/'20260907-5462-short-bound-witnesses/summary.json').read_text())
assert checked['checked_clean']==9
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm in ('candidate','kissat'):shutil.copy2(old/'pins'/arm,c/'pins'/arm)
(c/'run.py').write_text((old/'run.py').read_text().replace("'tactix-5463-'","'tactix-5471-'"))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((old/'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),input='benchmarks/cache/tierB/square.2.0.i.smt2-cvc4.cnf.xz',
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),stop_conflict=10000,
    hypothesis='Checked short square witness spends 3.188s inprocessing vs native 1.81s simplify, with substantial different BVE/Sweep/Factor work. Locate the first portable boundary on tested 5459 before attributing cost or editing a mechanism.',
    selection_authority='benchmarks/runs/20260907-5462-short-bound-witnesses/summary.json',
    validation='Two clean 10000-conflict controlled stops, below both checked complete conflict counts 17084/17876. Complete decision stream and rule-boundary comparison; no inferred weakness or acceptance timing.',
    limitation='Mechanism diagnostic on a short B case; not a replacement for fixed B28+C5 or Rank 1-40 competitiveness.')
m['input_sha256_compressed']=sha(root/m['input'])
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
