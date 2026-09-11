#!/usr/bin/env python3
"""Pin three matched Rank25/31 root-storage waves after the short causal control."""
import csv
import json
from pathlib import Path
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260911-7382-corrected-root-runtime';assert not c.exists()
gate=root/'benchmarks/runs/20260911-7380-corrected-root-pair'
raw=subprocess.check_output([sys.executable,str(Path(__file__).with_name('check-submit-corrected-pair-7380.py'))],text=True)
g=json.loads(raw[raw.index('{'):]);assert g['records']==2 and g['comparison']['storage_only']
m=json.loads((gate/'manifest.json').read_text());deps=dict(m['dependencies'])
def bind(p,expected=None):
    h=sha(p);assert expected is None or h==expected,p;deps[str(p.relative_to(root))]=h
for rel,digest in list(deps.items()):bind(root/rel,digest)
for rel,digest in m['pin_sha256'].items():bind(gate/rel,digest)
for p in (gate/'manifest.json',gate/'launch.json'):bind(p)
for t in range(2):
    p=gate/f'results/{t:03d}.json'
    if not p.exists():p=gate/f'provisional/{t:03d}.json'
    bind(p)
bc=root/'benchmarks/reports/2026-09-11-lucky-BC-classification-7381.json'
report=json.loads(bc.read_text());assert len(report['pairs'])==33;bind(bc)
for rel,digest in report['hashes'].items():bind(root/rel,digest)
fulls=list((root/'benchmarks/runs').glob('20260911-7312-*'));assert len(fulls)==1
full=fulls[0];fm=json.loads((full/'manifest.json').read_text());bind(full/'manifest.json')
inputs={}
for rank in (25,31):
    p=full/f'results/{rank-1:03d}.json';r=json.loads(p.read_text());assert classify(r,True)['checked']
    assert r['rank']==rank and r['expected']==('SAT' if rank==25 else 'UNSAT');bind(p);inputs[rank]=r
rows=[dict(task=t,rank=rank,arm=arm,tier='D',instance=inputs[rank]['instance'],expected=inputs[rank]['expected'])
      for t,(rank,arm) in enumerate((rank,arm) for wave in range(3) for rank in (25,31)
          for arm in (('folded','legacy') if wave==1 else ('legacy','folded')))]
for folder in ('pins','logs','claims','results','provisional'):(c/folder).mkdir(parents=True)
for p in (gate/'pins').iterdir():shutil.copy2(p,c/'pins'/p.name)
with (c/'matrix.tsv').open('x',newline='') as out:
    w=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
batch=(gate/'array.sbatch').read_text().replace(str(gate),str(c)).replace('7380-corrected-root-pair','7382-corrected-root-runtime').replace('--array=0-1\n','--array=0-11\n')
with (c/'array.sbatch').open('x') as out:out.write(batch)
for name in ('prepare-corrected-runtime-7382.py','check-submit-corrected-runtime-7382.py','root-storage-statistics-7350.py','occurrence-fingerprint-7256.py'):bind(Path(__file__).with_name(name))
manifest=dict(schema='corrected-root-runtime-7382-v1',arms=m['arms'],dependencies=deps,
    collect_runtime_context=True,input_sha256={r['instance']:fm['input_sha256'][r['instance']] for r in rows},
    resource_contract=dict(m['resource_contract'],array='0-11'),
    hypothesis='After correcting both guidance lifecycles, folding root storage preserves non-storage work and reduces hot variable storage. Does it actually reduce Rank25 peak memory or Rank31 runtime?',
    acceptance='All outcomes expected; exact own-repeat work/certificates. Cross-arm certificate and non-storage work exact under existing7350 conserved-slot classification, with every raw field retained. Stop new waves on changed work; investigate, not silently exclude. Independent proof/model checks and zero residue required for final acceptance.',
    measurement='Three independent four-task waves, middle arm order reversed, full width/no node constraint. Paired medians and entire ranges for solver wall time and RSS. Each arm complete runtime spread<=2% for stable headline; no minimum gain threshold. No proof-wait before next wave when provisional solver gate passes.',
    test_consideration='Both source builds pass full125 suites;7373 twice-run17 sanitizer fixtures and7374 independent144 models/proofs. Reader matrix and exact-work negative controls. No new solver or runner changes.',
    decision='Isolate storage from corrected lifecycle, not a new Kissat denominator or competitiveness claim. Preserve runtime and memory regressions; full50 still required before production promotion.')
manifest['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out:json.dump(manifest,out,indent=2)
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
