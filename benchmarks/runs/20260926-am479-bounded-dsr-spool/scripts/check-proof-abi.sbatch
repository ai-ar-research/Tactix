#!/usr/bin/env bash
#SBATCH --job-name=am478-r12-proof-check
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --time=00:30:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/scratch/am478-rank12-paired/logs/proof-check-%j.log
set -euo pipefail
ulimit -c 0
python3 - <<'PY'
import hashlib,json,os,pathlib,subprocess,tempfile,time,sys
root=pathlib.Path('/home/hwu/SAT-solving');sys.path.insert(0,str(root/'tools/bench'));from proof_oracle import dsr_proof_as_drat,certificate_route;run=root/'benchmarks/probes/scratch/am478-rank12-paired'
source=root/'benchmarks/cache/tierD24-26/8d4b50ec0cf99097e9ab0835937afee5-hwmcc17miters-xits-iso-6s281b35.sanitized.cnf.xz'
proof=run/'results/proof-abi-proof.dsr'
pins=root/'benchmarks/runs/20260926-am475-rank5-12-diagnostic/pins'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(8<<20),b''):h.update(chunk)
 return h.hexdigest()
assert sha(source)=='95b11c4ece1727b5ef1486f093f3237e91cd6717b4ff52ebffb82a59d82c8c4d'
assert sha(proof)=='0ea7cf3f4e124f11a923f804d5384fa6e5ca436ad57889861c4ad9a08458a967'
assert sha(pins/'drat-trim')=='f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a'
assert sha(pins/'cake_lpr')=='ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57'
with tempfile.TemporaryDirectory(prefix='am478-r12-check-',dir=os.environ.get('SLURM_TMPDIR','/tmp')) as d:
 p=pathlib.Path(d);cnf=p/'input.cnf';drat=p/'proof.drat';lpr=p/'proof.lpr'
 assert certificate_route(proof)=='drat'
 assert dsr_proof_as_drat(proof,drat)
 with cnf.open('wb') as out:subprocess.run(['xz','-dc',str(source)],stdout=out,check=True)
 stages=[]
 for name,command in [('drat-trim',[str(pins/'drat-trim'),str(cnf),str(drat),'-t','1500','-L',str(lpr)]),('cake_lpr',[str(pins/'cake_lpr'),str(cnf),str(lpr)])]:
  start=time.monotonic();r=subprocess.run(command,capture_output=True,text=True,timeout=1500)
  lines=(r.stdout+'\n'+r.stderr).splitlines();status=[x.strip() for x in lines if x.strip().startswith('s ')]
  row={'name':name,'exit_code':r.returncode,'wall_seconds':time.monotonic()-start,'status':status,'tail':lines[-15:]}
  stages.append(row);print(json.dumps(row),flush=True)
  assert r.returncode==0 and ('s VERIFIED' if name=='drat-trim' else 's VERIFIED UNSAT') in status,row
result={'schema':'am478-proof-abi-independent-check-v1','source_sha256':sha(source),'proof_sha256':sha(proof),'lpr_bytes':lpr.stat().st_size,'stages':stages}
(run/'results/proof-abi-check.json').write_text(json.dumps(result,indent=2)+'\n')
PY
