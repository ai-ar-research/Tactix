#!/usr/bin/env python3
"""Freeze a preregistered four-arm, two-family training experiment."""
from pathlib import Path
import datetime,hashlib,json,re,shutil,subprocess,sys
root=Path.cwd().resolve()
if root!=Path('/home/hwu/Tactix'):raise SystemExit('run inside authorized repository')
name=sys.argv[1]
if not re.fullmatch(r'shared-nonlinear-[a-z0-9-]+',name):raise SystemExit('invalid campaign name')
r=root/'benchmarks/runs'/name;pin=root/'build'/name
r.mkdir();pin.mkdir()
for child in ['pins','results','logs','scratch','tmp']:(r/child).mkdir()
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
commit=subprocess.check_output(['git','rev-parse','HEAD']).decode().strip()
subprocess.run(['git','archive','--format=tar','--output='+str(pin/'source.tar'),commit],check=True)
parents=['eth-e0-20261107','eth-training-supplement-20261107'];entries=[];provenance={}
for parent in parents:
 path=root/'benchmarks/runs'/parent/'corpus.json';d=json.loads(path.read_text());provenance[parent]=sha(path)
 for e in d['entries']:
  if e['split']=='train' and e['family'] in ['hardware-miters','lec-multipliers']:
   assert (root/e['cache_path']).is_file();entries.append(dict(e,source_corpus=parent,compressed_sha256=sha(root/e['cache_path'])))
assert len(entries)==28 and len({e['family'] for e in entries})==2
corpus=dict(schema='shared-nonlinear-training-v1',parents=provenance,entries=entries,selection='All twelve hardware-miters and all sixteen lec-multipliers already marked train; no outcome-based filtering')
(r/'corpus.json').write_text(json.dumps(corpus,indent=2)+'\n')
config=dict(schema='shared-nonlinear-four-arm-v1',frozen_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),commit=commit,source_archive_sha256=sha(pin/'source.tar'),build_pin=str(pin.relative_to(root)),corpus_sha256=sha(r/'corpus.json'),seconds=300,repetitions=3,arms=['baseline','definitions','priority','control'],resources=dict(cpus=1,memory_gib=32),proof_output=False,expected='SAT models independently checked; UNSAT independently unchecked during the user-authorized no-proof research phase',hypothesis='New shared nonlinear vocabulary in authenticated bounded gate cuts reduces end-to-end CDCL work beyond matched old-variable priority',parameters=dict(leaves=6,depth=4,terms=64,groups=32,consumers=8,cuts=200000,maximum_discovery_work=50000000,priority_dose='number of retained consumers per group'),timing='Full process wall time including input parsing, ordinary startup, snapshot, discovery, independent local truth validation, definitions, links and search. Shared decompression and original model validation recorded separately.',metric='Mean PAR-2 over all instance/repetition pairs within each family: completed solve wall seconds, otherwise 600. Never discard a timeout, invalid model, crash or missing task.',acceptance='Priority arm <= 0.75 * unchanged-baseline PAR-2 in EACH family; no paired baseline solve lost; every model valid; all tasks present; inspect definitions-only and matched existing-variable control before attributing a gain to new vocabulary.',retention='Inject after normal initial elimination and late Lucky, immediately before search. New variables reach the first search period before any further elimination; later normal inprocessing remains enabled.',pr_scope='Research branch only; public PR24 unchanged')
(r/'config.json').write_text(json.dumps(config,indent=2)+'\n')
for name in ['build.py','task.py','summarize.py']:shutil.copy2(root/'tools/nonlinear'/name,r/'pins'/name)
shutil.copy2(root/'tools/fuzz/modelcheck.py',r/'pins/modelcheck.py')
contract={p.name:sha(p) for p in (r/'pins').iterdir()};(r/'contract.json').write_text(json.dumps(contract,indent=2)+'\n')
for kind in ['build','measure']:
 body=f'''#!/bin/bash
#SBATCH --job-name=nonlinear-{kind}
#SBATCH --partition=cpu-q
#SBATCH --cpus-per-task={16 if kind=='build' else 1}
#SBATCH --mem={64 if kind=='build' else 32}G
#SBATCH --time={'01:00:00' if kind=='build' else '00:30:00'}
#SBATCH --chdir={root}
#SBATCH --output={r}/{kind}-%{'j' if kind=='build' else 'A_%a'}.log
'''
 if kind=='measure':body+='#SBATCH --array=0-83\n'
 body+=f'''set -euo pipefail
export TMPDIR={r}/tmp
export PYTHONDONTWRITEBYTECODE=1
python3.12 {r}/pins/{'build.py' if kind=='build' else 'task.py'} {r}'''
 if kind=='measure':body+=' "$SLURM_ARRAY_TASK_ID"'
 (r/(kind+'.sbatch')).write_text(body+'\n')
print(r)
