from pathlib import Path
import json,subprocess,hashlib,sys
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
records=[json.loads(line) for line in (run/'provenance.jsonl').read_text().splitlines()]
inputs=[r['clause'] for r in records if r['action']==2 and r['kind']=='snapshot-premise'];assert len(inputs)==16
active={};published=[]
for r in records:
 if r['action']!=2:continue
 if r['kind'] in ['define-xor-rat','derive-xor-rup']:assert r['id'] not in active;active[r['id']]=r['clause']
 if r['kind']=='delete-xor-helper':assert sorted(active.pop(r['id']))==sorted(r['clause'])
 if r['kind']=='publish-xor-consequence':assert active[r['id']]==r['clause'];published.append(r['clause'])
assert {tuple(sorted(c)) for c in published}=={(-1,2),(-2,1)}
assert all(all(abs(l)<=7 for l in c) for c in active.values())
text=(run/'nonterminal.dsr').read_text();literals=[abs(int(x)) for line in text.splitlines() for x in line.split() if x not in ['d','0']];g=max(literals)+1
cnf=run/'audit.cnf';proof=run/'audit.dsr';gadget=[[g,g+1],[g,-g-1],[-g,g+1],[-g,-g-1]]
cnf.write_text(f'p cnf {g+1} 20\n'+''.join(' '.join(map(str,c))+' 0\n' for c in inputs+gadget));proof.write_text(text+f'{g} 0\n0\n')
# Same fixed all-addition elaborator, with the closing unit's fresh ID adapted.
elab=run/'elaborate.py';elab.write_text((root/'benchmarks/runs/20261003-parity-nonterminal-module/elaborate.py').read_text().replace('c not in [[8],[]]',f'c not in [[{g}],[]]'))
commands=[]
def execute(name,args,kind=None):
 p=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=60)
 (run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr)
 status=oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr) if kind else None
 commands.append(dict(name=name,command=list(map(str,args)),returncode=p.returncode,status=status));assert p.returncode==0 or kind,p.stderr
 return status
execute('elaborate',[sys.executable,elab,cnf,proof,run/'audit.lrat'])
assert execute('all-addition',[checkers/'cake_lpr',cnf,run/'audit.lrat'],'cake_lpr')=='verified'
work=run/'canonical';work.mkdir(exist_ok=True);ok,stage,out=oracle.check_witness_free_dsr_as_drat(str(checkers/'drat-trim'),str(checkers/'cake_lpr'),cnf,proof,work,timeout=60);assert ok,(stage,out)
assert execute('forward',[checkers/'drat-trim',cnf,work/'proof.drat','-f'],'drat-trim')=='verified'
(run/'invalid.lrat').write_text('21 1 0 0\n');assert execute('negative',[checkers/'cake_lpr',cnf,run/'invalid.lrat'],'cake_lpr')=='rejected'
result=dict(scope='Fixed live internal adapter, not public controller acceptance',canonical_verified=True,published=published,all_helpers_retired=True,commands=commands,proof_bytes=proof.stat().st_size,bindings={str(p.relative_to(root)):sha(p) for p in [Path(__file__),elab,run/'nonterminal.dsr',run/'provenance.jsonl',checkers/'cake_lpr',checkers/'drat-trim']})
(run/'check-result.json').write_text(json.dumps(result,indent=2)+'\n');print('Live adapter proof and provenance lifecycle independently checked.')
