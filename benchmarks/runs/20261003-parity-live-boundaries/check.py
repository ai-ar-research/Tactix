from pathlib import Path
import json,subprocess,hashlib,sys
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
results=[]
for name in ['ternary','quaternary','unit','root-conflict','compacted-unit']:
 work=run/(name+'-check');work.mkdir()
 source=run/(name+'.cnf');native=run/(name+'.dsr');cnf=source;proof=native
 clauses=[list(map(int,line.split()[:-1])) for line in source.read_text().splitlines() if line and line[0] not in 'pc']
 n=len(clauses);text=native.read_text();g=None
 if name!='root-conflict':
  literals=[abs(int(x)) for line in text.splitlines() for x in line.split() if x not in ['d','0']];g=max(literals)+1
  gadget=[[g,g+1],[g,-g-1],[-g,g+1],[-g,-g-1]];cnf=work/'audit.cnf';proof=work/'audit.dsr'
  cnf.write_text(f'p cnf {g+1} {n+4}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses+gadget));proof.write_text(text+f'{g} 0\n0\n')
 elab=work/'elaborate.py';template=(root/'benchmarks/runs/20261003-parity-nonterminal-module/elaborate.py').read_text()
 if g is None:template=template.replace(' if c not in [[8],[]]: assert not any(abs(h) in range(17,21) for h in hints), (idx,c,hints)','')
 else:template=template.replace('c not in [[8],[]]',f'c not in [[{g}],[]]').replace('range(17,21)',f'range({n+1},{n+5})')
 elab.write_text(template)
 commands=[]
 def execute(label,args,kind=None):
  p=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=60)
  (work/(label+'.stdout')).write_text(p.stdout);(work/(label+'.stderr')).write_text(p.stderr)
  status=oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr) if kind else None
  commands.append(dict(label=label,command=list(map(str,args)),returncode=p.returncode,status=status));assert p.returncode==0 or kind,p.stderr
  return status
 execute('elaborate',[sys.executable,elab,cnf,proof,work/'all-addition.lrat'])
 assert execute('cake',[checkers/'cake_lpr',cnf,work/'all-addition.lrat'],'cake_lpr')=='verified'
 canonical=work/'canonical';canonical.mkdir();ok,stage,out=oracle.check_witness_free_dsr_as_drat(str(checkers/'drat-trim'),str(checkers/'cake_lpr'),cnf,proof,canonical,timeout=60);assert ok,(name,stage,out)
 assert execute('forward',[checkers/'drat-trim',cnf,canonical/'proof.drat','-f'],'drat-trim')=='verified'
 results.append(dict(case=name,canonical_verified=ok,commands=commands,original_cnf_sha256=sha(source),native_proof_sha256=sha(native),native_proof_bytes=native.stat().st_size,disjoint_closing_gadget=g is not None))
 print(name,'all proof checks passed',flush=True)
(run/'check-result.json').write_text(json.dumps({'cases':results,'bindings':{str(p.relative_to(root)):sha(p) for p in [Path(__file__),checkers/'cake_lpr',checkers/'drat-trim']}},indent=2)+'\n')
