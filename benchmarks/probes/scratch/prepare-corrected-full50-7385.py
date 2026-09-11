#!/usr/bin/env python3
"""Prepare the complete root-storage transfer; keep native and old campaigns frozen."""
import datetime as dt
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha,reference
from checked_comparison import classify
c=root/'benchmarks/runs/20260911-7385-corrected-root-ranks1-50';assert not c.exists()
evidence={}
def bind(p,expected=None):
    d=sha(p);assert expected is None or d==expected,p;evidence[str(p.relative_to(root))]=d
def read_module(name,file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
assessment=root/'benchmarks/reports/2026-09-11-corrected-root-assessment-7384.json'
a=json.loads(assessment.read_text());assert a['expected_outcomes']==12;bind(assessment)
for rel,digest in a['hashes'].items():bind(root/rel,digest)
assert len(a['cross_arm_classification'])==6 and all(p['comparison']['storage_only'] for p in a['cross_arm_classification'])
# Memory-priority investigation: every Rank25 pair must improve both metrics.
# Rank31 regressions remain explicit; this is not a no-regression acceptance.
for metric in ('wall_seconds','peak_rss_kib'):assert a['runtime']['25']['paired'][metric]['max']<1
for spread in a['spreads'].values():assert spread['absolute_headline_stable']==(max(spread['by_arm'].values())<=.02)
subprocess.run([sys.executable,str(Path(__file__).with_name('check-submit-corrected-runtime-7382.py'))],check=True)
bc=root/'benchmarks/runs/20260911-7375-lucky-BC'
subprocess.run([sys.executable,str(Path(__file__).with_name('check-submit-lucky-BC-7375.py'))],check=True,stdout=subprocess.DEVNULL)
classifier=read_module('lucky_classification','classify-lucky-BC-7381.py');classifier.fixtures()
bcrows={}
for t in range(66):
    p=bc/f'results/{t:03d}.json';final=p.exists()
    if not final:p=bc/f'provisional/{t:03d}.json'
    r=json.loads(p.read_text());assert classify(r,final)['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
    bind(p);bcrows[t]=r
for t in range(0,66,2):classifier.compare(bcrows[t],bcrows[t+1],{8:5,26:29,30:1,60:5}.get(t,0))
build_path=root/'benchmarks/runs/20260911-7373-lucky-reattach-build/manifest.json'
b=json.loads(build_path.read_text());assert b['status']=='PASS';bind(build_path)
for rel,digest in b['files'].items():bind(root/b['source']/rel,digest)
for group in b['binaries'].values():
    for rel,digest in group.items():bind(root/rel,digest)
assert list(b['binaries']['release'].values())==list(b['binaries']['bc'].values())
proof_path=root/'benchmarks/runs/20260911-7374-lucky-proofs/manifest.json';proof=json.loads(proof_path.read_text());bind(proof_path)
assert proof['status']=='PASS' and proof['checked_UNSAT']==72 and proof['checked_SAT']==72
old=root/'benchmarks/runs/20260911-7312-clause-realloc-ranks1-50'
m=json.loads((old/'manifest.json').read_text());bind(old/'manifest.json')
for rel,digest in m['pin_sha256'].items():bind(old/rel,digest)
ref=reference();assert len(ref['records'])==50 and abs(ref['solver_seconds']-7239.010)<1e-8
for task in range(50):
    p=old/f'results/{task:03d}.json';r=json.loads(p.read_text());assert classify(r,True)['checked'];bind(p)
for folder in ('pins','logs','results','provisional','claims'):(c/folder).mkdir(parents=True)
for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):shutil.copy2(old/'pins'/name,c/'pins'/name)
binary=root/'build/pin-7373-lucky-reattach-release/tactix'
patch=root/'benchmarks/runs/20260911-7383-lucky-lifecycle-checkpoint/cumulative.patch'
audit_path=root/'benchmarks/reports/2026-09-11-lucky-lifecycle-audit-7383.json'
audit=json.loads(audit_path.read_text());bind(audit_path)
bind(patch,audit['hashes'][str(patch.relative_to(root))])
shutil.copy2(binary,c/'pins/tactix');shutil.copy2(patch,c/'pins/candidate-source.patch');shutil.copy2(old/'matrix.tsv',c/'matrix.tsv')
batch=(old/'array.sbatch').read_text().replace(str(old),str(c)).replace('--job-name=runtime-checked','--job-name=7385-root-full50')
with (c/'array.sbatch').open('x') as out:out.write(batch)
arm=dict(m['arms']['tactix'],sha256=sha(binary),source_build=str(binary.parent.relative_to(root)),source_patch_sha256=sha(patch))
for name in ('prepare-corrected-full50-7385.py','submit-corrected-full50-7385.py','validate-corrected-full50-7385.py',
             'classify-lucky-BC-7381.py','assess-corrected-roots-7384.py','root-storage-statistics-7350.py','occurrence-fingerprint-7256.py'):bind(Path(__file__).with_name(name))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),arms={'tactix':arm},evidence_sha256=evidence,
    candidate_authority=dict(path=str(build_path.relative_to(root)),sha256=sha(build_path)),parent_comparison=str(old.relative_to(root)),
    hypothesis='Corrected7373 root storage across all50 after complete three-wave25/31 evidence. Measure aggregate runtime and all memory tradeoffs while preserving closed ranks. Parent7302 has stale-root guidance; changed work must not be labeled pure unit cost.',
    decision='All three Rank25 memory/runtime pairs improve; retain the complete Rank31 regression and all arm spreads. Full-set investigation before production promotion, not a minimum-gain filter or waiver of fuzz/correctness/memory gates.',
    test_consideration='Release125 twice/Trace124,17 sanitizer fixtures twice,144 original proof/model checks and two rejected controls. B28+C5 outcomes classified with exact explicit deactivation deltas. All12 targeted outcomes and six storage-only pairs. Existing full50 runner/matrix unchanged; fixed-runtime and7350 schema/changed-work controls.',
    validation='Validate identity, every final/provisional state and all models/proofs; no aggregate score with unclassified invalidity or solve loss. Retain every raw parent difference and classify exact/storage-only/changed-work-or-certificate. Changed work does not establish a unit-cost gain. Expected complete solver outcomes permit provisional scoring and development; checked-clean all50 required for acceptance.')
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as out:json.dump(m,out,indent=2)
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
