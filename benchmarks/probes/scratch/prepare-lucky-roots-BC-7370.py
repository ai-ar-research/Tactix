#!/usr/bin/env python3
"""Unchanged B28+C5 gate: frozen7340 versus policy root-publication repair."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
c=root/'benchmarks/runs/20260911-7370-lucky-roots-BC';assert not c.exists()
old=root/'benchmarks/runs/20260911-7347-root-storage-BC'
deps={}
def bind(path,expected=None):
    digest=sha(path);assert expected is None or digest==expected,path;deps[str(path.relative_to(root))]=digest
om=json.loads((old/'manifest.json').read_text());bind(old/'manifest.json')
for rel,digest in om['pin_sha256'].items():bind(old/rel,digest)
for task in range(66):
    p=old/f'results/{task:03d}.json';assert classify(json.loads(p.read_text()),True)['checked'];bind(p)
builds={}
for label,rel in [('current','20260911-7340-debug-contracts-build'),('candidate','20260911-7369-lucky-roots-validation')]:
    p=root/'benchmarks/runs'/rel/'manifest.json';b=json.loads(p.read_text());assert b['status']=='PASS';bind(p)
    for file,digest in b['files'].items():bind(root/b['source']/file,digest)
    for file,digest in b['hashes'].items():bind(root/file,digest)
    builds[label]=(p,b)
proof_path=root/'benchmarks/runs/20260911-7368-lucky-root-proofs/manifest.json'
proofs=json.loads(proof_path.read_text());assert proofs['status']=='PASS' and proofs['checked_UNSAT']==72 and proofs['checked_SAT']==72 and proofs['negative_controls']==2;bind(proof_path)
for rel,digest in proofs['hashes'].items():bind(root/rel,digest)
for folder in ('pins','logs','claims','results','provisional'):(c/folder).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):shutil.copy2(old/'pins'/name,c/'pins'/name)
shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
arms={}
for tier,kind in [('B','release'),('C','bc')]:
    for label,stem in [('current','7340-debug-contracts'),('candidate','7366-lucky-roots')]:
        p=root/f'build/pin-{stem}-{kind}/tactix';authority,b=builds[label]
        digest=b['binaries'][kind][str(p.relative_to(root))];bind(p,digest)
        arm=label+'_'+tier;shutil.copy2(p,c/'pins'/arm)
        arms[arm]=dict(path='pins/'+arm,sha256=digest,arguments=[],source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',source_build=str(p.parent.relative_to(root)),source_manifest=str(authority.relative_to(root)),source_state='Isolated source; source_commit is production lineage, not complete source. Manifest files own exact source.')
for label in ('current','candidate'):assert arms[label+'_B']['sha256']==arms[label+'_C']['sha256']
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('7347-root-storage-BC','7370-lucky-roots-BC')
with (c/'array.sbatch').open('x') as out:out.write(batch)
for p in (Path(__file__),Path(__file__).with_name('check-submit-lucky-roots-BC-7370.py'),
          root/'benchmarks/reports/2026-09-11-root-lifecycle-analysis-7367.json'):bind(p)
m=dict(schema='tactix-lucky-roots-BC-v1',arms=arms,input_sha256=om['input_sha256'],collect_runtime_context=True,
    resource_contract=om['resource_contract'],dependencies=deps,
    hypothesis='Consume newly proved root suffix after both DefaultPolicy Lucky calls. Eliminate stale fixed queue entries at their owning lifecycle before physical compaction/BVE, with no new threshold, branch scheduling or proof-rule restriction.',
    test_consideration='Actual-parent12-case negative regression; Release124 twice/Trace123;16 focused sanitizer fixtures twice;72 original UNSAT proofs/72 SAT models and two bad-proof rejections. All raw solver differences remain visible.',
    acceptance='All66 expected original outcomes and independent checked-clean proofs/models, no solve loss. Compare7340 parent, not production competitiveness. Classify every work/certificate change before acceptance; changes caused by the repaired lifecycle may improve or worsen search. No arbitrary field exclusions, parent-stamp offset or inherited gate waiver. Pending proof checks permit provisional development only.',
    next='Inspect short B14 regression and every changed pair. If lifecycle/short gate survives, measure Rank25/31 memory/runtime with required three independent paired waves before further full50. Keep fixed Kissat denominator.',
    pending='Latest-source fuzz/consolidation remain open. Full Debug7342 is an independent older-source process. No production promotion.')
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out:json.dump(m,out,indent=2)
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
