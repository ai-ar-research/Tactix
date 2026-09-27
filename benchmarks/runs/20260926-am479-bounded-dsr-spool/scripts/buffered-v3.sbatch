#!/usr/bin/env bash
#SBATCH --job-name=am479-r12-buffered-v3
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=00:30:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am478-rank12-paired/logs/buffered-v3-%A_%a.log
set -euo pipefail
ulimit -c 0
python3 - <<'PY'
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,time
root=pathlib.Path('/home/hwu/SAT-solving')
run=root/'benchmarks/probes/scratch/am478-rank12-paired'
source=root/'benchmarks/cache/tierD24-26/8d4b50ec0cf99097e9ab0835937afee5-hwmcc17miters-xits-iso-6s281b35.sanitized.cnf.xz'
binaries={'current':root/'benchmarks/runs/20260926-am475-rank5-12-diagnostic/pins/tactix',
          'ablation':root/'build/am479-proof-spool-v3/tactix'}
expected={'source':'95b11c4ece1727b5ef1486f093f3237e91cd6717b4ff52ebffb82a59d82c8c4d',
          'current':'c57482f1551e48b6f460398790b605b75cb9149e96d38e8406e2b1f2b5315631',
          'ablation':'09cd3e2f1c8fee62309fe89cc616e0b8a5892ddd879d6d23b788e7b4fb53c112',
          'current_proof':'c794f8d973045fecd759d3b64fade0e7e42a9bcc1eeaaa3d156a9ec9303620f8'}
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(8<<20),b''):h.update(chunk)
 return h.hexdigest()
assert sha(source)==expected['source']
for name,binary in binaries.items():assert sha(binary)==expected[name]
task=int(os.environ['SLURM_ARRAY_TASK_ID']);assert task in (0,1)
order=['ablation','current'] if task==0 else ['current','ablation']
with tempfile.TemporaryDirectory(prefix=f'am478-ablate-{task}-',dir=os.environ.get('SLURM_TMPDIR','/tmp')) as dirname:
 temp=pathlib.Path(dirname);cnf=temp/'input.cnf'
 with cnf.open('wb') as f:subprocess.run(['xz','-dc',str(source)],stdout=f,check=True)
 rows=[]
 for name in order:
  binary=binaries[name];stdout=temp/f'{name}.stdout';stderr=temp/f'{name}.stderr';proof=temp/f'{name}.proof'
  start=time.monotonic()
  with stdout.open('wb') as out,stderr.open('wb') as err:
   try:proc=subprocess.run([str(binary),str(cnf),str(proof)],stdout=out,stderr=err,timeout=300);rc=proc.returncode;timeout=False
   except subprocess.TimeoutExpired:rc=None;timeout=True
  wall=time.monotonic()-start;lines=stdout.read_text(errors='replace').splitlines()
  row={'name':name,'wall_seconds':wall,'exit_code':rc,'timed_out':timeout,'proof_sha256':sha(proof) if proof.exists() else None,
       'proof_bytes':proof.stat().st_size if proof.exists() else None,'timing':[x for x in lines if x.startswith(('c timing','c phasecost'))],
       'counters':[x for x in lines if x.startswith(('c congruence','c conggates','c conflicts','c decisions'))],
       'verdict':[x for x in lines if x.startswith('s ')],'stderr_tail':stderr.read_text(errors='replace')[-1000:]}
  rows.append(row)
  print(json.dumps({'name':name,'wall_seconds':round(wall,3),'exit_code':rc,'proof_sha256':row['proof_sha256']}),flush=True)
  assert not timeout and rc==20 and row['verdict']==['s UNSATISFIABLE'],row
  assert row['proof_sha256']==expected['current_proof'],row
  if name=='ablation' and task==0:
   destination=run/'results/buffered-v3-proof.dsr'
   shutil.copyfile(proof,destination)
 result={'schema':'am479-buffered-backend-pair-v3','task':task,'node':os.environ.get('SLURMD_NODENAME'),
         'input_sha256':expected['source'],'binary_sha256':{k:expected[k] for k in binaries},'order':order,'rows':rows}
 (run/'results'/f'buffered-v3-{task}.json').write_text(json.dumps(result,indent=2)+'\n')
PY
