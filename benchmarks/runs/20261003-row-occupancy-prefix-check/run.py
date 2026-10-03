from pathlib import Path
import json,hashlib,subprocess,sys,os,time
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
assert os.environ['SLURM_JOB_ID'];m=json.loads((run/'manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in m['bindings'].items():assert sha(root/p)==h,p
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers';outcomes=[]
for name,command,kind,want in [('all-addition',[str(checkers/'cake_lpr'),str(run/'audit.cnf'),str(run/'all-addition.lrat')],'cake_lpr','verified'),('forward',[str(checkers/'drat-trim'),str(run/'audit.cnf'),str(run/'audit.dsr'),'-f'],'drat-trim','verified'),('negative',[str(checkers/'cake_lpr'),str(run/'audit.cnf'),str(run/'negative.lrat')],'cake_lpr','rejected')]:
 start=time.monotonic();p=subprocess.run(command,capture_output=True,text=True,timeout=300)
 (run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr)
 got=oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr);outcomes.append({'name':name,'command':command,'status':got,'seconds':time.monotonic()-start});assert got==want,(name,got,p.stdout,p.stderr)
for p,h in m['bindings'].items():assert sha(root/p)==h,p
result={'job':os.environ['SLURM_JOB_ID'],'definition_additions':m['definition_additions'],'outcomes':outcomes,'scope':m['scope']}
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
