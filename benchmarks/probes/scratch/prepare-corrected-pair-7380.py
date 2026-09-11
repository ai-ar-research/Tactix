#!/usr/bin/env python3
"""Separate root storage from corrected Lucky lifecycle on the short witness."""
import csv
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260911-7380-corrected-root-pair';assert not c.exists()
gate=root/'benchmarks/runs/20260911-7375-lucky-BC';m=json.loads((gate/'manifest.json').read_text())
deps={}
def bind(p,expected=None):
    h=sha(p);assert expected is None or h==expected,p;deps[str(p.relative_to(root))]=h
for rel,digest in m['pin_sha256'].items():bind(gate/rel,digest)
for p in (gate/'manifest.json',gate/'launch.json'):bind(p)
oracle_path=gate/'results/027.json';oracle=json.loads(oracle_path.read_text());assert classify(oracle,True)['checked'];bind(oracle_path)
arms={}
for label,build_path,pin in [('legacy','20260911-7379-corrected-legacy-build','pin-7379-corrected-legacy-release'),('folded','20260911-7373-lucky-reattach-build','pin-7373-lucky-reattach-release')]:
    path=root/'benchmarks/runs'/build_path/'manifest.json';b=json.loads(path.read_text());assert b['status']=='PASS';bind(path)
    for rel,digest in b['files'].items():bind(root/b['source']/rel,digest)
    for rel,digest in b['hashes'].items():bind(root/rel,digest)
    binary=root/'build'/pin/'tactix';digest=sha(binary)
    expected=b['binaries'][str(binary.relative_to(root))] if label=='legacy' else b['binaries']['release'][str(binary.relative_to(root))]
    assert digest==expected
    arms[label]=dict(path='pins/'+label,sha256=digest,arguments=[],source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',source_manifest=str(path.relative_to(root)),source_build=str(binary.parent.relative_to(root)))
for folder in ('pins','logs','claims','results','provisional'):(c/folder).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):shutil.copy2(gate/'pins'/name,c/'pins'/name)
for label,info in arms.items():shutil.copy2(root/info['source_build']/'tactix',c/info['path'])
rows=[dict(task=i,rank=14,arm=arm,tier='B',instance=oracle['instance'],expected='UNSAT') for i,arm in enumerate(('legacy','folded'))]
with (c/'matrix.tsv').open('x',newline='') as out:
    w=csv.DictWriter(out,fieldnames=rows[0],delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
batch=(gate/'array.sbatch').read_text().replace(str(gate),str(c)).replace('7375-lucky-BC','7380-corrected-root-pair').replace('--array=0-65','--array=0-1')
with (c/'array.sbatch').open('x') as out:out.write(batch)
for p in (Path(__file__),Path(__file__).with_name('check-submit-corrected-pair-7380.py'),Path(__file__).with_name('root-storage-statistics-7350.py')):bind(p)
manifest=dict(schema='corrected-root-pair-7380-v1',arms=arms,input_sha256={oracle['instance']:m['input_sha256'][oracle['instance']]},collect_runtime_context=True,resource_contract=dict(m['resource_contract'],array='0-1'),dependencies=deps,folded_oracle=str(oracle_path.relative_to(root)),
    hypothesis='After both guidance lifecycles are repaired, legacy and folded root storage should produce identical complete non-storage work and certificates. This distinguishes genuine storage drift from the old accidental faster stale-queue schedule.',
    acceptance='Both original proofs checked/clean. Folded arm exact entire7375 own work/cert; legacy versus folded exact certificate and all non-storage work under unchanged7350 field classification with conserved physical+removed slots. All raw differences retained. No three-wave timing or memory-gain claim.',
    test_consideration='Both corrected source arms pass full suites; shared Lucky/reattachment fixtures and independent proofs. Only one existing DefaultPolicy compaction bool differs in the legacy diagnostic. Use existing7350 changed-work and schema negative controls.')
manifest['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out:json.dump(manifest,out,indent=2)
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
