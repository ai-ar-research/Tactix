from pathlib import Path
import hashlib,json,os,subprocess,time
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
assert os.environ['SLURM_JOB_ID'];index=int(os.environ['SLURM_ARRAY_TASK_ID'])
m=json.loads((run/'manifest.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in m['bindings'].items():assert sha(root/p)==h,p
old=root/m['parent']/('case-'+str(index));expected=json.loads((old/'result.json').read_text())
assert expected['verified'];work=run/('case-'+str(index));work.mkdir();prefix=work/'native'
command=[str(root/m['binary']),str(old/'original.cnf'),str(prefix),'public'];start=time.monotonic()
p=subprocess.run(command,capture_output=True,text=True,timeout=300)
(work/'stdout').write_text(p.stdout);(work/'stderr').write_text(p.stderr)
assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
stats=json.loads(p.stdout);assert stats['native_unsat']
assert prefix.with_suffix('.dsr').read_bytes()==(old/'certificate.dsr').read_bytes()
records=[json.loads(line) for line in prefix.with_suffix('.records.jsonl').read_text().splitlines()]
assert all(x['action']==41 for x in records)
assert records[0]['kind']=='action-context' and records[-1]['kind']=='action-complete'
assert records[0]['instance']==22 and records[1]['kind']=='action-parameters'
assert records[-1]['recognized']==1 and records[-1]['additions']==stats['additions']
premises={x['occurrence']:x['clause'] for x in records if x['kind']=='original-premise'}
source=[x for x in records if x['kind']=='k4-source'];edges={tuple(x['vertices']):x['literal'] for x in records if x['kind']=='edge-map'}
import itertools
used=[]
for s in source:
 positive=sorted(edges[pair] for pair in itertools.combinations(s['vertices'],2))
 assert premises[s['positive_occurrence']]==positive
 assert premises[s['negative_occurrence']]==sorted(-l for l in positive)
 used += [s['positive_occurrence'],s['negative_occurrence']]
assert sorted(used)==list(range(1,len(premises)+1))
known=set();counts={}
for record in records:
 kind=record['kind'];counts[kind]=counts.get(kind,0)+1
 if kind=='define-ite':
  assert record['auxiliary'] not in known
  for child in ('high','low'):assert record[child] in [-(1<<63),(1<<63)-1] or record[child] in known
  known.add(record['auxiliary'])
assert len(known)==stats['nodes'] and counts['inequality']==36 and counts['witness']==17
lines=prefix.with_suffix('.dsr').read_text().splitlines();assert sum(not s.startswith('d ') for s in lines)==stats['additions'];assert sum(s.startswith('d ') for s in lines)==stats['deletions']
result=dict(case=index,job=os.environ['SLURM_JOB_ID'],command=command,stats=stats,seconds=time.monotonic()-start,proof_byte_equal=True,source_occurrences_verified=len(used),event_counts=counts,proof_sha256=sha(prefix.with_suffix('.dsr')),scope='Public typed controller Ramsey action, same-byte proof and namespace/ID parity with native no-proof execution; not production acceptance')
for p,h in m['bindings'].items():assert sha(root/p)==h,p
(work/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
