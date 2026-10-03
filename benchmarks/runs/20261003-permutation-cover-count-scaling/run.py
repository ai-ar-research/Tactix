from pathlib import Path
import hashlib,json,os,subprocess,time
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
m=json.loads((run/'manifest.json').read_text());i=int(os.environ['SLURM_ARRAY_TASK_ID']);n=m['cases'][i]
for p,h in m['bindings'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
work=run/f'n{n}';work.mkdir();started=time.monotonic()
result=dict(n=n,job_id=os.environ['SLURM_JOB_ID'],array_task=i,scope=m['scope'])
try:
 p=subprocess.run([str(root/m['binary']),str(work/'matrix'),str(n)],capture_output=True,text=True,timeout=300)
 (work/'stdout').write_text(p.stdout);(work/'stderr').write_text(p.stderr)
 result.update(returncode=p.returncode,elapsed_seconds=time.monotonic()-started)
 if p.returncode==0:result['count']=json.loads(p.stdout)
except subprocess.TimeoutExpired as e:
 result.update(status='timeout',elapsed_seconds=time.monotonic()-started)
 (work/'stdout').write_bytes(e.stdout or b'');(work/'stderr').write_bytes(e.stderr or b'')
(run/f'result-{i}.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
