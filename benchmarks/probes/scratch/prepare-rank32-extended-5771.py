#!/usr/bin/env python3
"""Extend beyond exhausted cached native coverage; never refresh runtime baseline."""
import hashlib
import json
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[3]
runs=root / 'benchmarks/runs'
old=runs / '20260908-5769-reuse-target-rank32-prefix'
native=runs / '20260908-5597-factor-support-rank32-prefix'
c=runs / '20260908-5771-reuse-target-rank32-extended'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
v=json.loads((old / 'summary.json').read_text())
assert v['complete_prefix_identical'] and v['matching_rows']==40102204
m=json.loads((old / 'manifest.json').read_text())
for rel,digest in m['hashes'].items():assert sha(old / rel)==digest
nm=json.loads((native / 'manifest.json').read_text())
assert sha(native / 'pins/kissat')==nm['hashes']['pins/kissat']
c.mkdir(exist_ok=False)
for folder in ('pins','logs','results','traces'):(c / folder).mkdir()
shutil.copy2(old / 'pins/candidate',c / 'pins/candidate')
shutil.copy2(native / 'pins/kissat',c / 'pins/kissat')
(c / 'run.py').write_text((old / 'run.py').read_text().replace('tactix-5769-','tactix-5771-'))
batch=(old / 'array.sbatch').read_text().replace(str(old),str(c))
assert '#SBATCH --array=0-0' in batch
(c / 'array.sbatch').write_text(batch.replace('#SBATCH --array=0-0','#SBATCH --array=0-1'))
m.update(parent_campaign=str(old.relative_to(root)),stop_conflict=300000,stream_from=100000,
    common_decisions=40102204,
    parent_summary=dict(path=str((old / 'summary.json').relative_to(root)),sha256=sha(old / 'summary.json')),
    hypothesis='Current Rank32 full-run work regresses,while all40102204 native decisions through100K agree. Extend both traces to300K and locate the first later split;retain digest equality for the entire prior common prefix.',
    validation='Two clean300K stops,complete recorded suffixes,prior40102204-row literal-prefix digests equal frozen100K control. No final proof/verdict or acceptance timing.',
    native_rerun_reason='Missing diagnostic information beyond existing100K coverage,not routine runtime confirmation or a denominator replacement.',
    resource_contract=dict(partition='cpu-dense-hwulab-q',cpus_per_task=12,memory_mib=48000,array='0-1',throttle=None,node_constraint=None),
    limitation='Instrumented bounded diagnostic. Frozen native runtime vector remains6810.066s. Earlier control and new suffix have separate scopes.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [c / 'run.py',c / 'array.sbatch',c / 'pins/candidate',c / 'pins/kissat']}
(c / 'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c / 'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c,sha(c / 'manifest.json'))
