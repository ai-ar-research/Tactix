#!/usr/bin/env python3
"""5450 fixture review: run the immutable predecessor pin's in-process unit
test executables as they exist, without ctest (which would write into the
pin directory) and without any script-driven test that launches the solver
binary on CNF files or inspects SLURM/fuzz artefacts.  Read-only on the pin.
"""
from pathlib import Path
import subprocess
import sys
import time
root=Path(__file__).resolve().parents[3]
pin=root/'build/pin-5445-replacement-cursor-release/tests'
tests=[('kernel','test_kernel'),('parse','test_parse'),('proof','test_proof'),
    ('analyze','test_analyze'),('restart_reduce','test_restart_reduce'),
    ('switchmode','test_switchmode'),('rephase','test_rephase'),
    ('subsolver','test_subsolver'),('sweep','test_sweep'),
    ('scheduling','test_scheduling'),('strategy_config','test_strategy_config'),
    ('event_trace','test_event_trace'),('slot_pose','test_slot_pose'),
    ('reorder','test_reorder'),('walk','test_walk'),('subsume','test_subsume'),
    ('strengthen','test_strengthen'),('instantiate','test_instantiate'),
    ('vivify','test_vivify'),('vivify_n1','test_vivify_n1'),('probe','test_probe'),
    ('backbone','test_backbone'),('bce','test_bce'),('cce','test_cce'),
    ('ternary','test_ternary'),('hyperbinary','test_hyperbinary'),
    ('decide','test_decide'),('reconstruct','test_reconstruct'),
    ('substitute','test_substitute'),('congruence','test_congruence'),
    ('binary_deletion','test_binary_deletion'),
    ('transitive_reduce','test_transitive_reduce'),('bve','test_bve'),
    ('lucky','test_lucky'),('api_n1','test_api_c'),
    ('api_instances','test_api_instances'),('random_policy','test_random_policy'),
    ('tr_bruteforce_0','test_tr_bruteforce','0'),
    ('tr_bruteforce_1','test_tr_bruteforce','1'),
    ('tr_bruteforce_2','test_tr_bruteforce','2'),
    ('tr_bruteforce_3','test_tr_bruteforce','3'),
    ('factor','test_factor'),('factor_n1','test_factor_n1'),
    ('subsume_n1','test_subsume_n1'),('no_heuristics','test_no_heuristics'),
    ('solve_reset','test_solve_reset'),('inprocess_root','test_inprocess_root'),
    ('replacement_cursor','test_replacement_cursor'),('vivify_max','test_vivify_max')]
log=root/'benchmarks/runs/20260907-5450-eliminate-limit/logs/predecessor-unit-regression-5450-review.log'
lines=[]
failed=[]
for name,exe,*args in tests:
    start=time.time()
    try:
        r=subprocess.run([str(pin/exe),*args],cwd=pin,capture_output=True,
            text=True,timeout=900)
        rc=r.returncode
        tail=(r.stdout+r.stderr).strip().splitlines()[-1:]
    except subprocess.TimeoutExpired:
        rc='timeout'
        tail=[]
    status='PASS' if rc==0 else 'FAIL'
    if rc!=0:
        failed.append(name)
    line=f'{status} {name} rc={rc} {time.time()-start:.1f}s {tail[0] if tail else ""}'
    lines.append(line)
    print(line,flush=True)
summary=f'{len(tests)-len(failed)}/{len(tests)} predecessor unit executables passed; failed={failed}'
lines.append(summary)
print(summary)
log.write_text('\n'.join(lines)+'\n')
sys.exit(1 if failed else 0)
