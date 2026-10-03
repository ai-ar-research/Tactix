from pathlib import Path
import hashlib,json,subprocess,sys
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
pins=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
prior=json.loads((root/'benchmarks/runs/20261003-parity-public-action/check-result.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['cake_lpr','drat-trim']:
 p=pins/name;assert sha(p)==prior['bindings'][str(p.relative_to(root))]
# Fixed all-addition elaboration, with explicit exclusion of closing-gadget
# premises from every prefix justification. CakeLPR remains the authority.
source=(root/'benchmarks/runs/20261003-parity-public-action/elaborate.py').read_text()
source=source.replace('cnf,proof,out=map(Path,sys.argv[1:]);db={}',"cnf,proof,out=map(Path,sys.argv[1:4]);prefix_count=int(sys.argv[4]);forbidden=set(map(int,sys.argv[5:]));db={}")
source=source.replace('for line in proof.read_text().splitlines():','for ordinal,line in enumerate(proof.read_text().splitlines()):')
source=source.replace('if c not in [[24],[]]: assert not any(abs(h) in range(17,21) for h in hints), (idx,c,hints)','if ordinal < prefix_count: assert not any(abs(h) in forbidden for h in hints), (idx,c,hints)')
(run/'elaborate.py').write_text(source)
commands=[]
def execute(name,cmd,kind=None):
 p=subprocess.run(list(map(str,cmd)),text=True,capture_output=True,timeout=60)
 (run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr)
 status=oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr) if kind else None
 commands.append({'name':name,'command':list(map(str,cmd)),'exit':p.returncode,'status':status})
 if not kind:assert p.returncode==0,(name,p.stdout,p.stderr)
 return status
for name in ['odd','equivalence']:
 original=run/(name+'.cnf');native=run/(name+'.dsr');cnf=original;proof=native
 lines=native.read_text().splitlines();forbidden=[]
 if name=='equivalence':
  g=max(abs(int(x)) for line in lines for x in line.split())+1
  clauses=original.read_text().splitlines()[1:];assert len(clauses)==4
  gadget=[[g,g+1],[g,-g-1],[-g,g+1],[-g,-g-1]]
  cnf=run/'equivalence-audit.cnf';proof=run/'equivalence-audit.dsr'
  cnf.write_text('p cnf '+str(g+1)+' 8\n'+'\n'.join(clauses)+'\n'+''.join(' '.join(map(str,c))+' 0\n' for c in gadget))
  proof.write_text(native.read_text()+str(g)+' 0\n0\n');forbidden=list(range(5,9))
 lrat=run/(name+'.lrat');execute(name+'-elaborate',[sys.executable,run/'elaborate.py',cnf,proof,lrat,len(lines),*forbidden])
 assert execute(name+'-all-addition',[pins/'cake_lpr',cnf,lrat],'cake_lpr')=='verified'
 work=run/(name+'-canonical');work.mkdir(exist_ok=True)
 ok,stage,output=oracle.check_witness_free_dsr_as_drat(str(pins/'drat-trim'),str(pins/'cake_lpr'),cnf,proof,work,timeout=60)
 assert ok,(stage,output)
 assert execute(name+'-forward',[pins/'drat-trim',cnf,work/'proof.drat','-f'],'drat-trim')=='verified'
# The satisfiable original equivalence premises do not imply x1.
(run/'invalid.lrat').write_text('5 1 0 0\n')
assert execute('invalid-unit',[pins/'cake_lpr',run/'equivalence.cnf',run/'invalid.lrat'],'cake_lpr')=='rejected'
paths=[Path(__file__),run/'elaborate.py',run/'test',root/'src/proof/cardinality_certificate.hpp',root/'tests/test_cardinality_certificate.cpp',pins/'cake_lpr',pins/'drat-trim']
paths += [run/(name+suffix) for name in ['odd','equivalence'] for suffix in ['.cnf','.dsr','.definitions.jsonl']]
(run/'check-result.json').write_text(json.dumps({'scope':'Fixed shared counting microcontrols, not public integration or full acceptance','commands':commands,'canonical_both_verified':True,'prefix_gadget_hints_excluded':True,'sha256':{str(p.relative_to(root)):sha(p) for p in paths}},indent=2)+'\n')
print('Odd-degree refutation and nonterminal equivalence independently verified; false unit rejected')
