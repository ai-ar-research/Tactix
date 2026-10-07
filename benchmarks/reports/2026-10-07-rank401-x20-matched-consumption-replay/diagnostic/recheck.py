#!/usr/bin/env python3
"""Successor checker-only route. No solver execution and no original writes."""
import hashlib,json,resource,subprocess,time
from pathlib import Path
D=Path(__file__).resolve().parent
ROOT=D.parents[3]
M=json.loads((D/'bindings.json').read_text())
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    for path,digest in M['files'].items():assert sha(path)==digest,path
    resource.setrlimit(resource.RLIMIT_FSIZE,(134217728,)*2)
    output=D/'successor-results';output.mkdir(exist_ok=False)
    deadline=time.monotonic()+1800
    records=[];common=[]
    def run(name,cmd,out):
        remaining=deadline-time.monotonic();assert remaining>0
        with (out/(name+'.log')).open('wb') as log:
            p=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=remaining)
        text=(out/(name+'.log')).read_text()
        return {'command':list(map(str,cmd)),'exit':p.returncode,'log_sha256':sha(out/(name+'.log')),'tokens':[x for x in text.splitlines() if x.startswith('s ')]}
    for arm in ('off','on'):
        inp=Path(M['original'])/arm/'results';out=output/arm;out.mkdir()
        # Explicit -f preserves every lemma; -c chooses supported binary EOF parsing.
        forward=run('forward',[M['dsr_trim'],'-f','-c',str(inp/'checker.cnf'),str(inp/'proof.dsr'),str(out/'forward.binary.lsr')],out)
        assert forward['exit']==0 and forward['tokens']==['s VALID'],forward
        expand=run('expand',[M['compress'],'-d',str(out/'forward.binary.lsr'),str(out/'roundtrip.lsr')],out)
        assert expand['exit']==0,expand
        # All clause IDs, witnesses, hints, deletions and order must survive.
        assert (out/'roundtrip.lsr').read_bytes().split()==(inp/'forward.lsr').read_bytes().split(),'forward stream differs'
        independent=run('independent',[M['srcheck'],str(inp/'checker.cnf'),str(out/'forward.binary.lsr'),'c'],out)
        assert independent['exit']==0 and independent['tokens']==['s VALID'],independent
        binary=(out/'forward.binary.lsr').read_bytes();assert binary
        (out/'truncated.binary.lsr').write_bytes(binary[:-1])
        negative=run('negative',[M['srcheck'],str(inp/'checker.cnf'),str(out/'truncated.binary.lsr'),'c'],out)
        assert negative['tokens']==['s INVALID'],negative
        trace=(inp/'transcript').read_text().splitlines();boundary=trace.index('COMMON-END');common.append(trace[2:boundary])
        assert sum(x.startswith('owner-coordinate ') for x in common[-1])==3
        verdict=int(trace[-1].removeprefix('verdict '));assert trace[-1]=='verdict '+str(verdict) and verdict in (0,10)
        if verdict==10:
            model=run('model',[str(ROOT/'tools/fuzz/checkmodel'),str(inp/'input.cnf'),str(inp/'stdout')],out)
            assert model['exit']==0,model
        else:model=None
        installed=[x for x in trace if x.startswith('X20-installed ')];assert len(installed)==(3 if arm=='on' else 0)
        records.append({'arm':arm,'stages':[forward,expand,independent,negative],'original_proof_sha256':sha(inp/'proof.dsr'),'binary_lsr_sha256':sha(out/'forward.binary.lsr'),'whole_prefix_valid':True,'verdict':verdict,'model':model,'end':next(x for x in trace if x.startswith('END ')),'installed':installed,'direct_bcp':sum(x.startswith('X20-BCP ') for x in trace),'direct_conflicts':sum(x.startswith('X20-direct-conflict ') for x in trace)})
    assert common[0]==common[1],'common prefix divergence'
    (output/'successor-grade.json').write_text(json.dumps({'records':records,'common_prefix_identical':True,'source':M['source'],'UNKNOWN_prefix_verdict_credit':False,'original_jobs_remain_failed':True,'long_run_authorized':False,'binary_id_limitation':'Recorded ID1 is DSR presence sentinel, not unique proof ID; exact pair and immediate non-jumped reason support narrow direct use only', 'scope':'Independent full-prefix checker successor only; no new solver evidence'},indent=2)+'\n')
if __name__=='__main__':main()
