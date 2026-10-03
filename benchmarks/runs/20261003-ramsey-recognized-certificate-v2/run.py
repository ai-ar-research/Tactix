import hashlib,itertools,json,lzma,os,random,subprocess,sys,time
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
import proof_oracle as oracle
run=Path(__file__).resolve().parent
m=json.loads((run/'manifest.json').read_text())
assert os.environ['SLURM_JOB_ID']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in m['bindings'].items():assert sha(root/p)==h,p
index=int(os.environ['SLURM_ARRAY_TASK_ID']);case=m['cases'][index]
work=run/('case-'+str(index));work.mkdir()
started=time.monotonic()
result={'case':case,'job':os.environ['SLURM_JOB_ID'],'verified':False}
def save():
 (work/'result.json').write_text(json.dumps(result,indent=2)+'\n')
save()
with lzma.open(root/m['input'],'rb') as src:original=src.read()
lines=[s for s in original.decode().splitlines() if s and not s.startswith('c')]
assert lines[0].split()==['p','cnf','171','7752']
clauses=[];row=[]
for token in ' '.join(lines[1:]).split():
 x=int(token)
 if x:row.append(x)
 else:clauses.append(row);row=[]
assert not row
old_edges={e:i+1 for i,e in enumerate(itertools.combinations(range(19),2))}
expected=[tuple(sorted(sign*old_edges[e] for e in itertools.combinations(vs,2))) for sign in (1,-1) for vs in itertools.combinations(range(19),4)]
assert sorted(map(lambda r:tuple(sorted(r)),clauses))==sorted(expected)
cnf=work/'original.cnf'
if case['variant']=='original':
 cnf.write_bytes(original);mapping={i:i for i in range(1,172)}
else:
 n=case['vertices'];rng=random.Random(case['seed'])
 selected=[old_edges[e] for e in itertools.combinations(range(n),2)]
 names=[3*i+7 for i in range(len(selected))];rng.shuffle(names)
 mapping={v:names[i]*(-1 if rng.randrange(2) else 1) for i,v in enumerate(selected)}
 transformed=[]
 for row in clauses:
  if any(abs(x) not in mapping for x in row):continue
  row=[mapping[abs(x)]*(1 if x>0 else -1) for x in row];rng.shuffle(row);transformed.append(row)
 rng.shuffle(transformed)
 cnf.write_text('p cnf '+str(max(map(abs,mapping.values())))+' '+str(len(transformed))+'\n'+''.join(' '.join(map(str,row))+' 0\n' for row in transformed))
 assert len(transformed)==2*len(list(itertools.combinations(range(n),4)))
(work/'mapping.json').write_text(json.dumps(mapping,indent=2)+'\n')
prefix=work/'certificate';binary=root/m['binary']
with (work/'generator.stdout').open('w') as out,(work/'generator.stderr').open('w') as err:
 completed=subprocess.run([str(binary),str(prefix),str(cnf)],stdout=out,stderr=err,timeout=300)
result.update(generator_exit=completed.returncode,input_sha256=sha(cnf));save()
assert completed.returncode==0
assert prefix.with_suffix('.cnf').read_bytes()==cnf.read_bytes()
result['generation']=json.loads((work/'generator.stdout').read_text());save()
proof=prefix.with_suffix('.dsr')
# Verify recovered sources independently against the exact input sequence.
input_rows=[]
for line in cnf.read_text().splitlines():
 if line.startswith(('c','p')):continue
 input_rows.append(tuple(sorted(map(int,line.split()[:-1]))))
edges={};sources=[]
for line in prefix.with_suffix('.recognition.jsonl').read_text().splitlines():
 item=json.loads(line)
 if item['kind']=='edge':edges[tuple(sorted((item['a'],item['b'])))]=item['literal']
 if item['kind']=='k4':sources.append(item)
used=set()
for source in sources:
 for sign,key in ((1,'positive_source'),(-1,'negative_source')):
  identifier=source[key];assert identifier not in used;used.add(identifier)
  expected=tuple(sorted(sign*edges[tuple(e)] for e in itertools.combinations(source['vertices'],2)))
  assert input_rows[identifier]==expected
assert used==set(range(len(input_rows)))
result['source_occurrences_checked']=len(used);save()
original_run=oracle.run_captured;counter=0
def logged(command,*args,**kwargs):
 global counter
 at=counter;counter+=1;t=time.monotonic()
 with (work/'checker-events.jsonl').open('a') as f:f.write(json.dumps({'event':'start','index':at,'command':list(command)})+'\n')
 try:done=original_run(command,*args,**kwargs)
 except Exception as e:
  with (work/'checker-events.jsonl').open('a') as f:f.write(json.dumps({'event':'exception','index':at,'error':str(e),'seconds':time.monotonic()-t})+'\n')
  raise
 (work/('checker-'+str(at)+'.stdout')).write_text(done.stdout)
 (work/('checker-'+str(at)+'.stderr')).write_text(done.stderr)
 with (work/'checker-events.jsonl').open('a') as f:f.write(json.dumps({'event':'end','index':at,'exit':done.returncode,'seconds':time.monotonic()-t})+'\n')
 return done
oracle.run_captured=logged
pins=run/'checkers';check=work/'check';check.mkdir();t=time.monotonic()
ok,stage,output=oracle.check_dsr(str(pins/'dsr-trim'),str(pins/'dsr-compress'),str(pins/'srcheck'),cnf,proof,check,timeout=1800,every_lemma=True)
result.update(verified=ok,checker_stage=stage,checker_output=output,check_seconds=time.monotonic()-t,proof_bytes=proof.stat().st_size,proof_sha256=sha(proof));save()
assert ok,(stage,output[-2000:])
# Positive setup and a genuinely false proof must get distinct oracle outcomes.
badcnf=work/'negative.cnf';badproof=work/'negative.dsr'
badcnf.write_text('p cnf 4 6\n1 2 0\n1 3 0\n1 4 0\n2 3 0\n2 4 0\n3 4 0\n');badproof.write_text('0\n')
badcheck=work/'negative-check';badcheck.mkdir()
bad,badstage,badoutput=oracle.check_dsr(str(pins/'dsr-trim'),str(pins/'dsr-compress'),str(pins/'srcheck'),badcnf,badproof,badcheck,timeout=60,every_lemma=True)
assert not bad and badstage=='dsr-trim' and 'Could not derive the empty clause during backwards checking' in badoutput,(badstage,badoutput)
forward=oracle.run_captured([str(pins/'dsr-trim'),'-f',str(badcnf),str(badproof),str(work/'negative-forward.lsr')],timeout=60)
assert forward.returncode==1 and "Didn't UP refute the empty clause" in forward.stderr,(forward.returncode,forward.stdout,forward.stderr)
result.update(negative_rejected=True,negative_stage=badstage,negative_output=badoutput);save()
# Recognition corruption controls must decline before opening a proof stream.
base_rows=[list(r) for r in input_rows]
for kind in ('missing','duplicate','sign'):
 rows=[r[:] for r in base_rows]
 if kind=='missing':rows.pop()
 if kind=='duplicate':rows[0]=rows[1][:]
 if kind=='sign':rows[0][0]=-rows[0][0]
 bad=work/(kind+'.cnf');p=work/kind
 bad.write_text('p cnf '+str(max(abs(v) for r in rows for v in r))+' '+str(len(rows))+'\n'+''.join(' '.join(map(str,r))+' 0\n' for r in rows))
 done=subprocess.run([str(binary),str(p),str(bad)],capture_output=True,text=True,timeout=300)
 (work/(kind+'.stdout')).write_text(done.stdout);(work/(kind+'.stderr')).write_text(done.stderr)
 assert done.returncode==2 and not p.with_suffix('.dsr').exists(),(kind,done.returncode)
result.update(recognition_negative_controls=3,total_seconds=time.monotonic()-started)
for p,h in m['bindings'].items():assert sha(root/p)==h,p
save();print(json.dumps(result,indent=2))
