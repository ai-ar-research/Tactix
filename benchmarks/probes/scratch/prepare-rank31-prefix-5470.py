#!/usr/bin/env python3
"""Follow the new complete-view largest loss with an immutable causal probe."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
runs=root/'benchmarks/runs'
old=runs/'20260907-5463-rank37-bound-prefix'
c=runs/'20260907-5470-rank31-bound-prefix'
losses=json.loads((runs/'20260907-5454-eliminate-limit-ranks1-40/next-losses.json').read_text())
witness=losses['ranked_losses'][0];assert witness['rank']==31
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm in ('candidate','kissat'):shutil.copy2(old/'pins'/arm,c/'pins'/arm)
(c/'run.py').write_text((old/'run.py').read_text().replace("'tactix-5463-'","'tactix-5470-'"))
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
m=json.loads((old/'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),input='benchmarks/cache/'+witness['instance'],
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    hypothesis='Rank 31 is now the largest complete 5454 loss: 418.509/264.298 seconds, conflicts 1.542264x Kissat. Localize its earliest portable semantic boundary on tested bound candidate 5459 before selecting a general repair.',
    selection_authority='benchmarks/runs/20260907-5454-eliminate-limit-ranks1-40/next-losses.json',
    validation='Two clean 100000-conflict controlled stops; all decision rows, normalized boundary fields, pin/trace identities and scratch cleanup. Changes may be stronger or neutral; do not infer a defect from divergence alone.',
    limitation='Diagnostic-only unfuzzed investigation material; does not replace queued B28+C5, full Rank 1-40 or correctness gates.')
m['input_sha256_compressed']=sha(root/m['input'])
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
