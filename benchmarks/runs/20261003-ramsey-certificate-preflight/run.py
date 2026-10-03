from pathlib import Path
import hashlib,json,os,subprocess,time
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
assert os.environ['SLURM_JOB_ID'];index=int(os.environ['SLURM_ARRAY_TASK_ID'])
m=json.loads((run/'manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in m['bindings'].items():assert sha(root/p)==h,p
old=root/m['parent']/('case-'+str(index));expected=json.loads((old/'result.json').read_text())
assert expected['verified'];work=run/('case-'+str(index));work.mkdir();prefix=work/'preflight'
command=[str(root/m['binary']),str(prefix),str(old/'original.cnf')];start=time.monotonic()
p=subprocess.run(command,capture_output=True,text=True,timeout=300)
(work/'stdout').write_text(p.stdout);(work/'stderr').write_text(p.stderr)
assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
stats=json.loads(p.stdout)
assert stats['preflight_matches'] and stats['bounded_declines']==4 and stats['interrupted_phases']==7
assert prefix.with_suffix('.cnf').read_bytes()==(old/'original.cnf').read_bytes()
assert prefix.with_suffix('.dsr').read_bytes()==(old/'certificate.dsr').read_bytes()
proof_lines=prefix.with_suffix('.dsr').read_text().splitlines()
adds=sum(not line.startswith('d ') for line in proof_lines);deletes=len(proof_lines)-adds
assert adds==stats['physical_additions'] and deletes==stats['physical_deletions']
records=[json.loads(line) for line in prefix.with_suffix('.events.jsonl').read_text().splitlines()]
known=set();inequalities=[];logical=[];counts={}
for record in records:
 kind=record['kind'];counts[kind]=counts.get(kind,0)+1
 if kind=='define-ite':
  assert record['auxiliary'] not in known and record['auxiliary']>stats['variables']
  assert 0<record['condition']<=stats['variables']
  for child in ['high','low']:assert record[child] in [-(1<<63),(1<<63)-1] or record[child] in known
  known.add(record['auxiliary'])
 if kind=='inequality':
  assert record['root'] in [-(1<<63),(1<<63)-1] or record['root'] in known
  identities=[term[0] for term in record['terms']]
  assert identities==sorted(set(identities)) and all(v>0 and c for v,c in record['terms'])
  inequalities.append(record)
 if 'logical_id' in record:logical.append(record['logical_id'])
assert logical==list(range(logical[0],logical[0]+len(logical)))
assert len(known)==stats['nodes'] and len(inequalities)==stats['inequalities']==36
assert counts['sr']==counts['witness']==stats['sr_events']==17
assert records[-1]['kind']=='add' and records[-1]['clause']==[]
result={'case':index,'job':os.environ['SLURM_JOB_ID'],'stats':stats,'command':command,'seconds':time.monotonic()-start,'proof_byte_equal':True,'input_byte_equal':True,'physical_event_counts_match':True,'semantic_record_counts':counts,'source_provenance_authority':str((old/'certificate.recognition.jsonl').relative_to(root)),'proof_sha256':sha(prefix.with_suffix('.dsr')),'scope':'Pure complete derivation preflight with research output adapter; no native action or production acceptance'}
for p,h in m['bindings'].items():assert sha(root/p)==h,p
(work/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
