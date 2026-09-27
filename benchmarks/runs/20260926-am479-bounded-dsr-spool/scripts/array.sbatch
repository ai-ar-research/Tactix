#!/usr/bin/env bash
#SBATCH --job-name=am478-r12-paired
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=00:30:00
#SBATCH --array=0-1
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am478-rank12-paired/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
python3 - <<'PY'
import hashlib,json,os,pathlib,re,subprocess,tempfile,time
root=pathlib.Path('/home/hwu/SAT-solving')
run=root/'benchmarks/probes/scratch/am478-rank12-paired'
source=root/'benchmarks/cache/tierD24-26/8d4b50ec0cf99097e9ab0835937afee5-hwmcc17miters-xits-iso-6s281b35.sanitized.cnf.xz'
old=root/'benchmarks/runs/20260923-am337-proof-lifetime-rank1-50/pins/tactix'
new=root/'benchmarks/runs/20260926-am475-rank5-12-diagnostic/pins/tactix'
expected={'source':'95b11c4ece1727b5ef1486f093f3237e91cd6717b4ff52ebffb82a59d82c8c4d',
          'old':'40b0de228744fb878d6e9cb478babd3fc589eb944b01b52f93e59475516df40c',
          'new':'c57482f1551e48b6f460398790b605b75cb9149e96d38e8406e2b1f2b5315631',
          'old_proof':'c4a0f1fd2f42de9857df99eb7e107ae373e301ecf4787f0ecd0f9ac586363913',
          'new_proof':'c794f8d973045fecd759d3b64fade0e7e42a9bcc1eeaaa3d156a9ec9303620f8'}
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(8<<20),b''):h.update(chunk)
 return h.hexdigest()
assert sha(source)==expected['source']
assert sha(old)==expected['old'] and sha(new)==expected['new']
task=int(os.environ['SLURM_ARRAY_TASK_ID'])
assert task in (0,1)
order=['old_auto','new_auto','old_none','new_none'] if task==0 else ['new_auto','old_auto','new_none','old_none']
with tempfile.TemporaryDirectory(prefix=f'am478-r12-{task}-',dir=os.environ.get('SLURM_TMPDIR','/tmp')) as dirname:
 temp=pathlib.Path(dirname)
 cnf=temp/'input.cnf'
 with cnf.open('wb') as f:subprocess.run(['xz','-dc',str(source)],stdout=f,check=True)
 rows=[]
 for label in order:
  arm,mode=label.split('_')
  binary={'old':old,'new':new}[arm]
  stdout=temp/f'{label}.stdout';stderr=temp/f'{label}.stderr';proof=temp/f'{label}.proof'
  command=[str(binary),str(cnf)]+([str(proof)] if mode=='auto' else [])
  start=time.monotonic()
  with stdout.open('wb') as out,stderr.open('wb') as err:
   try:result=subprocess.run(command,stdout=out,stderr=err,timeout=300);rc=result.returncode;timed_out=False
   except subprocess.TimeoutExpired:rc=None;timed_out=True
  wall=time.monotonic()-start
  lines=stdout.read_text(errors='replace').splitlines()
  timing=[line for line in lines if line.startswith('c timing') or line.startswith('c phasecost')]
  counters=[line for line in lines if line.startswith(('c congruence','c conggates','c conflicts','c decisions'))]
  row={'label':label,'binary_sha256':expected[arm],'wall_seconds':wall,'exit_code':rc,'timed_out':timed_out,
       'proof_sha256':sha(proof) if proof.exists() else None,'proof_bytes':proof.stat().st_size if proof.exists() else None,
       'timing':timing,'counters':counters,'verdict_lines':[line for line in lines if line.startswith('s ')],
       'stderr_tail':stderr.read_text(errors='replace')[-1000:]}
  rows.append(row)
  print(json.dumps({'label':label,'wall_seconds':round(wall,3),'exit_code':rc,'proof_sha256':row['proof_sha256']}),flush=True)
  assert not timed_out and rc==20 and row['verdict_lines']==['s UNSATISFIABLE'],row
  if mode=='auto':
   assert row['proof_sha256']==expected[f'{arm}_proof'],row
   proof.unlink()
 result={'schema':'am478-rank12-sequential-pair-v1','task':task,'node':os.environ.get('SLURMD_NODENAME'),
         'job_id':os.environ.get('SLURM_ARRAY_JOB_ID'),'input_sha256':expected['source'],'order':order,'rows':rows}
 (run/'results'/f'{task}.json').write_text(json.dumps(result,indent=2)+'\n')
PY
