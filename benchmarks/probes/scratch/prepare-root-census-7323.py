#!/usr/bin/env python3
"""Pin Rank25 source-census and exact Release control; preserve all old runs."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c=root/'benchmarks/runs/20260911-7323-rank25-root-census'
assert not c.exists(),'prepared campaign frozen'
prior=root/'benchmarks/runs/20260911-7315-rank25-native-memory'
m=json.loads((prior/'manifest.json').read_text())
bpath=root/'benchmarks/runs/20260911-7322-root-census-build/manifest.json'
b=json.loads(bpath.read_text());assert b['status']=='PASS'
evidence={}
def bind(p,digest=None):
 actual=sha(p);assert digest is None or actual==digest,p
 evidence[str(p.relative_to(root))]=actual
bind(bpath);bind(prior/'manifest.json');bind(prior/'summary.json')
for rel,d in b['hashes'].items():bind(root/rel,d)
for name in ('pins','logs','claims','provisional','results'):(c/name).mkdir(parents=True)
for name in ('runtime_task.py','syscall_memory_task.py','modelcheck.py','drat-trim','checkmodel','strace'):
 bind(prior/'pins'/name,m['pin_sha256']['pins/'+name]);shutil.copy2(prior/'pins'/name,c/'pins'/name)
shutil.copy2(prior/'pins/run.py',c/'pins/proc_memory_task.py')
shutil.copy2(root/'tools/bench/root_census_task.py',c/'pins/root_census_task.py')
shutil.copy2(Path(__file__).with_name('run-root-census-7323.py'),c/'pins/run.py')
shutil.copy2(prior/'matrix.tsv',c/'matrix.tsv')
oracle=m['release_records']['candidate'];m['arms']={};m['release_records']={}
for arm,kind in (('candidate','observer'),('previous','control')):
 binary=b['binaries'][kind];bind(root/binary['path'],binary['sha256'])
 shutil.copy2(root/binary['path'],c/'pins'/arm)
 m['arms'][arm]=dict(path='pins/'+arm,sha256=binary['sha256'],arguments=[],solver='tactix',
  source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',source_build=str(Path(binary['path']).parent))
 m['release_records'][arm]=dict(oracle)
with (c/'array.sbatch').open('x') as f:f.write((prior/'array.sbatch').read_text().replace(str(prior),str(c)).replace('7315-rank25-native-memory','7323-rank25-root-census'))
for name in ('prepare-root-census-7323.py','run-root-census-7323.py','validate-root-census-7323.py'):
 bind(Path(__file__).with_name(name))
for name in ('root_census.py','test_root_census.py','root_census_task.py','test_root_census_task.py'):
 bind(root/'tools/bench'/name)
m.update(schema='tactix-rank25-root-census-v1',created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
 evidence_sha256=evidence,
 hypothesis=b['hypothesis'],acceptance=b['gate']+' Both original SAT models independently checked and zero residue.',
 test_consideration='7322 both full suites, exact OFF binary, root-state/dense-hole/read-only/emission/suppression fixtures. Fourteen external wrapper/parser tests reject missing/duplicate/truncated/inconsistent records and preserve raw HWM decreases.',
 limitation='Diagnostic source observer versus exact7302, not a scored runtime pair/native refresh. Require2484880KiB own Release peak reproduction before attributing peak-time state. No source promotion, schedule/budget/proof changes or older campaign mutations.')
m['pin_sha256']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch']}
with (c/'manifest.json').open('x') as f:json.dump(m,f,indent=2)
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(c)
