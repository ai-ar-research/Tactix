from pathlib import Path
import hashlib,json,os,subprocess,time
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
index=int(os.environ['SLURM_ARRAY_TASK_ID']);m=json.loads((run/'manifest.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in m['bindings'].items():assert sha(root/p)==h,p
old=root/m['parent']/f'case-{index}';work=run/f'case-{index}';work.mkdir();prefix=work/'policy'
assert json.loads((old/'result.json').read_text())['verified']
command=[str(root/m['binary']),str(old/'original.cnf'),str(prefix)];start=time.monotonic()
p=subprocess.run(command,capture_output=True,text=True,timeout=300)
(work/'stdout').write_text(p.stdout);(work/'stderr').write_text(p.stderr)
assert p.returncode==0,(p.returncode,p.stdout[-2000:],p.stderr)
result=json.loads(p.stdout.splitlines()[-1]);assert result['policy_paths_verified']
certificates={}
for policy in ['default','random']:
 for replay in ['', '-replay']:
  proof=Path(str(prefix)+f'-{policy}'+replay+'.dsr')
  assert proof.read_bytes()==(old/'certificate.dsr').read_bytes()
  certificates[proof.name]=sha(proof)
 for replay in ['', '-replay','-no-proof']:
  records=[json.loads(s) for s in Path(str(prefix)+f'-{policy}'+replay+'.records.jsonl').read_text().splitlines()]
  actions=[x for x in records if x['kind']=='action-context']
  assert len(actions)==(2 if policy=='random' else 1)
  assert all(x['instance']==22 for x in actions)
  assert records[-1]['kind']=='action-complete' and records[-1]['recognized']==1
  assert len([x for x in records if x['kind']=='original-premise'])==len([x for x in (old/'original.cnf').read_text().splitlines() if x and x[0] not in 'cp'])
for p,h in m['bindings'].items():assert sha(root/p)==h,p
result.update(case=index,array=os.environ['SLURM_ARRAY_JOB_ID'],job=os.environ['SLURM_JOB_ID'],command=command,seconds=time.monotonic()-start,certificate_sha256=certificates,scope='Both actual policy loops, no-proof namespace parity, deterministic feedback replay and random choice replay; byte equality to independent prior checks, no new checker run')
(work/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
