"""Compute-node proof/model and abnormal-process checks for anonymous scratch."""
import json
import os
from pathlib import Path
import signal
import sys
from anonymous_scratch import AnonymousScratch
import runtime_task as runtime

c=Path(sys.argv[1]).resolve()
assert os.environ.get('SLURM_JOB_ID')
m=json.loads((c/'manifest.json').read_text())
arm=m['arms']['tactix']
certificate=arm.get('certificate','drat')
runtime.validate_runtime_pin_contract(c,m)
for rel,expected in m['pin_sha256'].items():
    assert runtime.sha256(c/rel)==expected,rel
checks=[]
for verdict,formula in [('SAT','p cnf 2 1\n1 2 0\n'),('UNSAT','p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n')]:
    s=AnonymousScratch()
    try:
        cnf=s/'input.cnf';proof=s/('proof.out' if certificate=='auto' else 'proof.dsr' if certificate=='dsr' else 'proof.drat');out=s/'solver.stdout';err=s/'solver.stderr'
        cnf.write_text(formula)
        result=runtime.measured([c/arm['path'],*arm['arguments'],cnf,proof],out,err,s/'time',30)
        assert result['exit_code']=={'SAT':10,'UNSAT':20}[verdict],result
        assert runtime.solver_summary(out)[0]==verdict
        if verdict=='SAT':
            checked=runtime.measured([c/'pins/checkmodel',cnf,out],s/'checker.out',s/'checker.err',s/'check.time',30)
            assert checked['exit_code']==0,(checked,runtime.tail(s/'checker.out'))
        else:
            checked=runtime.check_unsat_proof(
                c,certificate,cnf,proof,s,30,
                reported_route=runtime.solver_reported_route(out))
            assert checked['result']=='verified',checked
        assert all(os.fstat(fd).st_nlink==0 for fd in s.handles.values())
        checks.append(dict(verdict=verdict,solve=result,check=checked))
    finally:s.close()
    assert not s.exists() and not cnf.exists() and not proof.exists()
for code in ["import os;os._exit(7)","import time;time.sleep(60)"]:
    s=AnonymousScratch()
    try:
        result=runtime.measured([sys.executable,'-c',code],s/'out',s/'err',s/'time',.1)
        assert result['exit_code']==7 or result['timed_out'],result
        checks.append(result)
    finally:s.close()
with (c/'validation.json').open('x') as f:json.dump(dict(passed=True,job_id=os.environ['SLURM_JOB_ID'],node=os.uname().nodename,checks=checks),f,indent=2)
print(f'PASS: native solver SAT/model, UNSAT/{certificate.upper()}, crash and timeout; all scratch anonymous')
