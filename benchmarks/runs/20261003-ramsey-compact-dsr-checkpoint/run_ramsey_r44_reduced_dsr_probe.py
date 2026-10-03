import hashlib,itertools,json,lzma,os,subprocess,sys,time
from pathlib import Path
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
import proof_oracle as oracle
run=Path(sys.argv[1]).resolve();assert os.environ['SLURM_JOB_ID']
m=json.loads((run/'manifest.json').read_text())
for p,h in m['bindings'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
work=run/'case-00';work.mkdir();cnf=work/'original.cnf'
with lzma.open(root/m['input'],'rb') as src:cnf.write_bytes(src.read())
lines=[line for line in cnf.read_text().splitlines() if line and not line.startswith('c')]
assert lines[0].split()==['p','cnf','171','7752']
actual=[];clause=[]
for token in ' '.join(lines[1:]).split():
 x=int(token)
 if x:clause.append(x)
 else:actual.append(tuple(sorted(clause)));clause=[]
assert not clause
edges={e:i+1 for i,e in enumerate(itertools.combinations(range(19),2))}
expected=[tuple(sorted(sign*edges[e] for e in itertools.combinations(vs,2)))
          for sign in (1,-1) for vs in itertools.combinations(range(19),4)]
assert sorted(actual)==sorted(expected)
prefix=work/'ramsey';started=time.monotonic()
with (work/'generator.stdout').open('w') as out,(work/'generator.stderr').open('w') as err:
 done=subprocess.run([str(root/m['binary']),str(prefix),'19',str(cnf)],stdout=out,stderr=err,timeout=300)
result=dict(rank=630,scope=m['scope'],job_id=os.environ['SLURM_JOB_ID'],generator_returncode=done.returncode,verified=False)
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n');assert done.returncode==0
proof=prefix.with_suffix('.dsr');assert prefix.with_suffix('.cnf').read_bytes()==cnf.read_bytes()
parent=(root/m['parent_proof']).read_bytes()
original_clauses=set(actual)
kept=[];deletions=0
for line in proof.read_bytes().splitlines(keepends=True):
 fields=line.split()
 if fields and fields[0]==b'd' and len(fields)==8 and tuple(sorted(map(int,fields[1:-1]))) in original_clauses:
  deletions+=1
 else:kept.append(line)
assert deletions==7626 and b''.join(kept)==parent, 'unexpected change beyond original-clause deletions'
result.update(only_original_clause_deletions_changed=True,added_deletions=deletions)
# Stage events make canonical and every-lemma progress observable without
# modifying the oracle or inspecting another owner's campaign.
original_run=oracle.run_captured
counter=0
def logged_run(command,*args,**kwargs):
 global counter
 index=counter;counter+=1;begin=time.monotonic()
 with (work/'checker-events.jsonl').open('a') as log:
  log.write(json.dumps(dict(event='start',index=index,command=list(command)))+'\n')
 try:
  done=original_run(command,*args,**kwargs)
 except Exception as error:
  with (work/'checker-events.jsonl').open('a') as log:
   log.write(json.dumps(dict(event='exception',index=index,error=str(error),seconds=time.monotonic()-begin))+'\n')
  raise
 (work/f'checker-{index:02d}.stdout').write_text(done.stdout)
 (work/f'checker-{index:02d}.stderr').write_text(done.stderr)
 with (work/'checker-events.jsonl').open('a') as log:
  log.write(json.dumps(dict(event='end',index=index,returncode=done.returncode,seconds=time.monotonic()-begin))+'\n')
 return done
oracle.run_captured=logged_run

result.update(generation=json.loads((work/'generator.stdout').read_text()),proof_bytes=proof.stat().st_size,input_sha256=hashlib.sha256(cnf.read_bytes()).hexdigest())
pins=run/'checkers';check=work/'check';check.mkdir();checkstarted=time.monotonic()
ok,stage,output=oracle.check_dsr(str(pins/'dsr-trim'),str(pins/'dsr-compress'),str(pins/'srcheck'),cnf,proof,check,timeout=1800,every_lemma=True)
result.update(verified=ok,checker_stage=stage,checker_output=output,check_seconds=time.monotonic()-checkstarted)
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n');assert ok,(stage,output[-2000:])
badcnf=work/'negative.cnf';badproof=work/'negative.dsr';badcnf.write_text('p cnf 1 1\n1 0\n');badproof.write_text('0\n');badcheck=work/'negative-check';badcheck.mkdir()
bad,badstage,badoutput=oracle.check_dsr(str(pins/'dsr-trim'),str(pins/'dsr-compress'),str(pins/'srcheck'),badcnf,badproof,badcheck,timeout=60,every_lemma=True)
result.update(negative_rejected=not bad,negative_stage=badstage,negative_output=badoutput,total_seconds=time.monotonic()-started,proof_sha256=hashlib.sha256(proof.read_bytes()).hexdigest())
assert not bad
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
