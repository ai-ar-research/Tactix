from pathlib import Path
import hashlib,json,subprocess,sys
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
pin=root/'build/pin-parity-premise-selection-426832f4-01';pin.mkdir(exist_ok=True);assert not (pin/'manifest.json').exists()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
commands=[]
def execute(name,args,kind=None):
 p=subprocess.run([str(a) for a in args],capture_output=True,text=True,timeout=60)
 (run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr)
 status=oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr) if kind else None
 commands.append(dict(name=name,command=list(map(str,args)),returncode=p.returncode,status=status))
 assert p.returncode==0 or kind,p.stderr
 return status
flags=['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-Wall','-Wextra','-I'+str(root/'src')]
for name in ['test_parity_premises','test_parity_selection']:
 execute(name+'-compile',flags+[root/'tests'/f'{name}.cpp','-o',pin/name]);execute(name,[pin/name])
execute('probe-compile',flags+[run/'probe.cpp','-o',pin/'probe']);execute('probe',[pin/'probe',run/'audit.cnf',run/'proof.dsr',run/'premises.jsonl'])
elaborator=root/'benchmarks/runs/20261003-parity-nonterminal-module/elaborate.py'
execute('elaborate',[sys.executable,elaborator,run/'audit.cnf',run/'proof.dsr',run/'proof.lrat'])
assert execute('all-addition',[checkers/'cake_lpr',run/'audit.cnf',run/'proof.lrat'],'cake_lpr')=='verified'
work=run/'canonical';work.mkdir()
ok,stage,out=oracle.check_witness_free_dsr_as_drat(str(checkers/'drat-trim'),str(checkers/'cake_lpr'),run/'audit.cnf',run/'proof.dsr',work,timeout=60);assert ok,(stage,out)
assert execute('forward',[checkers/'drat-trim',run/'audit.cnf',work/'proof.drat','-f'],'drat-trim')=='verified'
(run/'invalid.lrat').write_text('21 1 0 0\n');assert execute('negative',[checkers/'cake_lpr',run/'audit.cnf',run/'invalid.lrat'],'cake_lpr')=='rejected'
paths=[root/'src/rules/lucky'/n for n in ['parity_premises.hpp','parity_selection.hpp','parity_certificate.hpp']]+[root/'tests'/f'{n}.cpp' for n in ['test_parity_premises','test_parity_selection']]+[Path(__file__),run/'probe.cpp',elaborator,checkers/'cake_lpr',checkers/'drat-trim',pin/'probe']
result=dict(scope='Fixed clause-backed XOR recognition/selection/proof pipeline; no live action or target acceptance',commands=commands,canonical_verified=ok,proof_bytes=(run/'proof.dsr').stat().st_size,bindings={str(p.relative_to(root)):sha(p) for p in paths})
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n');(pin/'manifest.json').write_bytes((run/'result.json').read_bytes());print('All fixed tests, every-addition proof checks and negative control passed.')
