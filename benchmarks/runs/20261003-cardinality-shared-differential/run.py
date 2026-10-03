from pathlib import Path
import hashlib,json,os,subprocess,time
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
assert os.environ['SLURM_JOB_ID'];index=int(os.environ['SLURM_ARRAY_TASK_ID'])
m=json.loads((run/'manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in m['bindings'].items():assert sha(root/p)==h,p
parent=root/m['parent'];old=parent/('case-'+str(index));expected=json.loads((old/'result.json').read_text())
assert expected['verified']
work=run/('case-'+str(index));work.mkdir();prefix=work/'shared'
command=[str(root/m['binary']),str(prefix),str(old/'original.cnf')];start=time.monotonic()
p=subprocess.run(command,capture_output=True,text=True,timeout=300)
(work/'stdout').write_text(p.stdout);(work/'stderr').write_text(p.stderr)
assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
generation=json.loads(p.stdout)
assert sha(prefix.with_suffix('.cnf'))==expected['input_sha256']
assert prefix.with_suffix('.dsr').read_bytes()==(old/'certificate.dsr').read_bytes()
assert prefix.with_suffix('.recognition.jsonl').read_bytes()==(old/'certificate.recognition.jsonl').read_bytes()
known=set();records=[]
for line in prefix.with_suffix('.definitions.jsonl').read_text().splitlines():
 r=json.loads(line);assert 0<r['condition']<=generation['variables']
 assert r['auxiliary']>generation['variables']+1 and r['auxiliary'] not in known
 for key in ['high','low']:assert r[key] in [-(1<<63),(1<<63)-1] or r[key] in known
 known.add(r['auxiliary']);records.append(r)
assert len(records)==generation['bdd_nodes']==generation['definition_records']
result={'case':index,'job':os.environ['SLURM_JOB_ID'],'generation':generation,'command':command,'seconds':time.monotonic()-start,'proof_byte_equal':True,'input_byte_equal':True,'recognition_provenance_byte_equal':True,'definition_dependencies_checked':len(records),'proof_sha256':sha(prefix.with_suffix('.dsr')),'scope':'Exact-byte correspondence to independently checked prior proof; no checker rerun or public-action acceptance'}
for p,h in m['bindings'].items():assert sha(root/p)==h,p
(work/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
