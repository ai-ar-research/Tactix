import hashlib,json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
checker=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
commands=[]
def execute(name,args,kind=None):
 p=subprocess.run(args,capture_output=True,text=True,timeout=60)
 (run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr)
 status=oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr) if kind else None
 commands.append({'name':name,'command':args,'exitcode':p.returncode,'status':status})
 assert p.returncode==0 or kind,p.stderr
 return status
pin=root/'build/pin-parity-nonterminal-proof-c6dd5f8f-01';pin.mkdir(exist_ok=True)
assert not (pin/'manifest.json').exists()
execute('compile',['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-Wall','-Wextra','-I'+str(root/'src'),str(root/'tests/test_parity_certificate.cpp'),'-o',str(pin/'test')])
execute('test',[str(pin/'test'),str(run/'audit.cnf'),str(run/'proof.dsr')])
execute('elaborate',[sys.executable,str(run/'elaborate.py'),str(run/'audit.cnf'),str(run/'proof.dsr'),str(run/'proof.lrat')])
assert execute('all-addition',[str(checker/'cake_lpr'),str(run/'audit.cnf'),str(run/'proof.lrat')],'cake_lpr')=='verified'
work=run/'canonical';work.mkdir(exist_ok=True)
ok,stage,out=oracle.check_witness_free_dsr_as_drat(str(checker/'drat-trim'),str(checker/'cake_lpr'),run/'audit.cnf',run/'proof.dsr',work,timeout=60)
(run/'canonical-output.txt').write_text(str(out));assert ok,(stage,out)
assert execute('forward',[str(checker/'drat-trim'),str(run/'audit.cnf'),str(work/'proof.drat'),'-f'],'drat-trim')=='verified'
(run/'invalid.lrat').write_text('21 1 0 0\n')
assert execute('negative',[str(checker/'cake_lpr'),str(run/'audit.cnf'),str(run/'invalid.lrat')],'cake_lpr')=='rejected'
paths=[root/'src/rules/lucky/parity_certificate.hpp',root/'src/rules/lucky/parity_refutation.cpp',root/'tests/test_parity_certificate.cpp',Path(__file__),run/'elaborate.py',checker/'drat-trim',checker/'cake_lpr']
result={'scope':'Reusable proof-only module, not a live action or target acceptance','proof_bytes':(run/'proof.dsr').stat().st_size,'canonical_verified':ok,'commands':commands,'bindings':{str(p.relative_to(root)):sha(p) for p in paths},'test_binary_sha256':sha(pin/'test')}
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('All proof checks and negative control passed;',result['proof_bytes'],'bytes')

(pin/'manifest.json').write_bytes((run/'result.json').read_bytes())
