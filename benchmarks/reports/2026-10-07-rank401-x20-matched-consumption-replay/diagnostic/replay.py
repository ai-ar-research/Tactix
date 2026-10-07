#!/usr/bin/env python3
"""Prepared SLURM-only replay of unchanged driver, with full-prefix checks."""
import hashlib,json,os,resource,subprocess,sys,time
from pathlib import Path
D=Path(__file__).resolve().parent;M=json.loads((D/'bindings.json').read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
for path,digest in M['files'].items():assert sha(path)==digest,path
assert os.environ.get('SLURM_JOB_ID'),'all solver replays require SLURM'
arm=sys.argv[1];assert arm in ('off','on')
old=Path(M['original']);inp=old/arm/'results';out=D/('replay-'+arm);out.mkdir(exist_ok=False)
resource.setrlimit(resource.RLIMIT_FSIZE,(134217728,)*2)
cmd=[str(old/'driver'),str(inp/'input.cnf'),str(out/'proof.dsr'),str(out/'provenance.jsonl'),str(inp/'transcript'),arm,str(old/'off.program'),'30','replay']
with (out/'stdout').open('wb') as stdout,(out/'stderr').open('wb') as stderr:
    process=subprocess.run(cmd,stdout=stdout,stderr=stderr,timeout=35)
assert process.returncode==0,'replay diverged/failed'
# Public replay checked every event against immutable original transcript.
# Bind byte-identical proof/provenance, rather than infer determinism from verdict.
assert sha(out/'proof.dsr')==sha(inp/'proof.dsr'),'proof stream diverged'
assert sha(out/'provenance.jsonl')==sha(inp/'provenance.jsonl'),'provenance diverged'
deadline=time.monotonic()+1800
stages=[]
def stage(name,cmd):
    remaining=deadline-time.monotonic();assert remaining>0
    with (out/(name+'.log')).open('wb') as log:p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=remaining)
    tokens=[x for x in (out/(name+'.log')).read_text().splitlines() if x.startswith('s ')]
    item={'name':name,'command':cmd,'exit':p.returncode,'tokens':tokens,'log_sha256':sha(out/(name+'.log'))};stages.append(item);return item
f=stage('forward',[M['dsr_trim'],'-f','-c',str(inp/'checker.cnf'),str(out/'proof.dsr'),str(out/'forward.binary.lsr')]);assert f['exit']==0 and f['tokens']==['s VALID']
e=stage('expand',[M['compress'],'-d',str(out/'forward.binary.lsr'),str(out/'roundtrip.lsr')]);assert e['exit']==0
assert (out/'roundtrip.lsr').read_bytes().split()==(inp/'forward.lsr').read_bytes().split()
s=stage('independent',[M['srcheck'],str(inp/'checker.cnf'),str(out/'forward.binary.lsr'),'c']);assert s['exit']==0 and s['tokens']==['s VALID']
(out/'truncated.binary.lsr').write_bytes((out/'forward.binary.lsr').read_bytes()[:-1])
n=stage('negative',[M['srcheck'],str(inp/'checker.cnf'),str(out/'truncated.binary.lsr'),'c']);assert n['tokens']==['s INVALID']
(out/'result.json').write_text(json.dumps({'arm':arm,'solver_command':cmd,'driver_sha256':sha(old/'driver'),'exact_transcript_replay':True,'byte_identical_proof_and_provenance':True,'stages':stages,'UNKNOWN_verdict_credit':False,'long_run_authorized':False},indent=2)+'\n')
