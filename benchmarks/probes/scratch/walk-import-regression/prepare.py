from pathlib import Path
import csv,json,hashlib,shutil,datetime,importlib.util
root=Path(__file__).resolve().parents[4];work=Path(__file__).resolve().parent
base=root/'benchmarks/runs/20261003-parity-public-default-rank1-50'
run=root/'benchmarks/runs/20261003-walk-import-regression-factorial';run.mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name in ('pins','claims','logs','results','provisional'):(run/name).mkdir()
# Preserve the maintained pinned runtime and exact independently checked oracles.
for p in (base/'pins').iterdir():
 if p.is_file() and p.name not in ('tactix','baseline.json','candidate-pin.json'):shutil.copyfile(p,run/'pins'/p.name);shutil.copymode(p,run/'pins'/p.name)
builds=json.loads((work/'builds.json').read_text())
with (base/'matrix.tsv').open() as f:original=list(csv.DictReader(f,delimiter='\t'))[44]
rows=[];arms={}
for i,b in enumerate(builds):
 pin=root/b['pin'];binary=run/'pins'/b['name'];shutil.copyfile(pin/'tactix',binary);shutil.copymode(pin/'tactix',binary)
 assert sha(binary)==b['binary_sha256']
 shutil.copyfile(pin/'manifest.json',run/'pins'/(b['name']+'-build.json'))
 rows.append(dict(original,task=str(i),arm=b['name']))
 arms[b['name']]=dict(path=str(binary.relative_to(run)),sha256=sha(binary),source_build=b['pin'],arguments=['--auto-proof'],certificate='auto')
with (run/'matrix.tsv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]),delimiter='\t',lineterminator='\n');w.writeheader();w.writerows(rows)
m=json.loads((base/'manifest.json').read_text());m['schema']='tactix-walk-import-diagnostic-v1';m['arms']=arms;m['input_sha256']={original['instance']:m['input_sha256'][original['instance']]};m['created_at']=datetime.datetime.now(datetime.timezone.utc).isoformat()
m['resource_contract'].update(partition='cpu-q',array='0-3')
m['purpose']='Diagnose failed Rank45 proof-cost gate; 2x2 private saved-gap seed versus ordinary target-gap seed, and seed-preparation subtraction versus preserving flip allowance. No production adoption or regression replacement.'
m['hypotheses']=['Preparation work subtraction changes the completed flip prefix and hence later CDCL trajectory.','Legacy saved-to-target copying changed unspecified extension-variable seed phases; emulate only the private seed, without mutating CDCL saved/best state.']
m['decisions']='First require current/current arm to reproduce frozen candidate proof hash/work; then compare work/proof identity and checker outcomes. A reduced proof does not alone authorize a production effort/profile change or waive full gates.'
m['limitation']='One affected instance, four candidate-derived diagnostic arms; no historical reference binary rerun. keep-flips diagnostic exceeds the original total dose by measured preparation work and is not an eligible production profile.'
m['acceptance_runtime_seconds']=None
job=(base/'array.sbatch').read_text().replace(str(base),str(run)).replace('--array=0-49','--array=0-3').replace('--partition=cpu-dense-hwulab-q','--partition=cpu-q').replace('--job-name=controller-rank1-50','--job-name=walk-import-diagnostic')
(run/'array.sbatch').write_text(job)
m['pin_sha256']={str(p.relative_to(run)):sha(p) for p in (run/'pins').iterdir() if p.is_file()};m['pin_sha256'].update({n:sha(run/n) for n in ('matrix.tsv','array.sbatch')})
(run/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
import sys
sys.path.insert(0,str(run/'pins'))
import runtime_task
runtime_task.validate_runtime_pin_contract(run,m)
print(run)
