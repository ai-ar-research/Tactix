"""Compute-node proof/model and abnormal-process checks for the campaign scratch.

Anonymous /tmp files by default; named files under the campaign's declared
scratch directory for the home-directory scratch policy.
"""
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
arm=next(iter(m['arms'].values()))
certificate=arm.get('certificate','drat')
runtime.validate_runtime_pin_contract(c,m)
home=m.get('scratch_policy')==runtime.HOME_SCRATCH_POLICY
runtime.CAKE_HEAP_MIB=runtime.cake_heap_mib(m)
scratch_count=0
def new_scratch():
    global scratch_count
    if not home:
        return AnonymousScratch()
    from directory_scratch import DirectoryScratch
    scratch_count+=1
    s=DirectoryScratch(runtime.home_scratch_root(c,m),
                       f"validation-{os.environ['SLURM_JOB_ID']}-{scratch_count}")
    os.environ['TMPDIR']=str(s.tmpdir)
    return s
for rel,expected in m['pin_sha256'].items():
    assert runtime.sha256(c/rel)==expected,rel
checks=[]
for verdict,formula in [('SAT','p cnf 2 1\n1 2 0\n'),('UNSAT','p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n')]:
    s=new_scratch()
    try:
        proof_name=('proof.out' if certificate=='auto' else 'proof.dsr' if certificate=='dsr' else 'proof.drat')
        cnf=s/'input.cnf';proof=s/proof_name;out=s/'solver.stdout';err=s/'solver.stderr'
        cnf.write_text(formula)
        argv=[c/arm['path'],*arm['arguments'],cnf]+([] if certificate=='none' else [proof])
        result=runtime.measured(argv,out,err,s/'time',30)
        assert result['exit_code']=={'SAT':10,'UNSAT':20}[verdict],result
        assert runtime.solver_summary(out)[0]==verdict
        if verdict=='SAT':
            if certificate!='none':
                certificate_bytes=proof.stat().st_size
                certificate_sha256=runtime.sha256(proof)
                assert s.release(proof_name)
            else:
                s.release(proof_name)
            assert s.contains('input.cnf') and s.contains('solver.stdout')
            checked=runtime.measured([c/'pins/checkmodel',cnf,out],s/'checker.out',s/'checker.err',s/'check.time',30)
            assert checked['exit_code']==0,(checked,runtime.tail(s/'checker.out'))
            if certificate!='none':
                assert certificate_bytes >= 0 and len(certificate_sha256)==64
            for name in ('input.cnf','solver.stdout','checker.out','checker.err','check.time'):
                s.release(name)
        elif certificate=='none':
            # No proof path was given: nothing to check, nothing may be written.
            assert not (home and proof.exists())
            checked=dict(result='unchecked-no-certificate')
            for name in ('input.cnf','solver.stdout',proof_name):
                s.release(name)
        else:
            reported_route=runtime.solver_reported_route(out)
            s.release('solver.stdout')
            checked=runtime.check_unsat_proof(
                c,certificate,cnf,proof,s,30,
                reported_route=reported_route,release_intermediates=True,
                cnf_scratch_name='input.cnf',proof_scratch_name=proof_name)
            assert checked['result']=='verified',checked
        for name in ('solver.stderr','time'):
            s.release(name)
        assert not s.exists()
        checks.append(dict(verdict=verdict,solve=result,check=checked))
    finally:s.close()
    assert not s.exists() and not cnf.exists() and not proof.exists()
for code in ["import os;os._exit(7)","import time;time.sleep(60)"]:
    s=new_scratch()
    try:
        result=runtime.measured([sys.executable,'-c',code],s/'out',s/'err',s/'time',.1)
        assert result['exit_code']==7 or result['timed_out'],result
        checks.append(result)
    finally:s.close()
with (c/'validation.json').open('x') as f:json.dump(dict(passed=True,job_id=os.environ['SLURM_JOB_ID'],node=os.uname().nodename,checks=checks),f,indent=2)
print(f'PASS: native solver SAT/model, UNSAT/{certificate.upper()}, crash and timeout; '
      f'all scratch {"in the campaign scratch directory, removed" if home else "anonymous"}')
