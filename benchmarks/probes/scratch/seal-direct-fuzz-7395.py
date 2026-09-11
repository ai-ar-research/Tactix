#!/usr/bin/env python3
"""Terminal successor to frozen7394; require all five jobs and actual coverage."""
from pathlib import Path

base=Path(__file__).with_name('audit-direct-fuzz-7394.py')
code=base.read_text()
changes={
    "2026-09-11-direct-fuzz-audit-7394.json":"2026-09-11-direct-fuzz-final-7395.json",
    "assert state in ('PENDING','RUNNING','COMPLETED'),(job,result.stdout)":
        "assert state == 'COMPLETED',(job,result.stdout)",
    "supplement=dict(job_id=sl['job_id'],status='PENDING_VALIDATION')":
        "supplement=dict(job_id=sl['job_id'],status='PENDING_VALIDATION')\nassert (S/'result.json').exists()\naccount=subprocess.run(['sacct','-n','-X','-j',sl['job_id'],'-S',sl['accounting_since'],'-o','JobID,State%30,ExitCode','-P'],text=True,capture_output=True,check=True)\naccount_rows=[r.split('|') for r in account.stdout.splitlines() if r.split('|')[0]==sl['job_id']]\nassert len(account_rows)==1 and account_rows[0][1].strip()=='COMPLETED' and account_rows[0][2]=='0:0'\nsupplement['accounting']=account.stdout\nassert not any((S/'failures').iterdir()), 'retained failure requires diagnosis'\nbind(S/'logs'/(sl['job_id']+'.log'),own=True)",
    "supplement.update(status='SOLVER_CHECKS_PASS'":"supplement.update(status='PASS'",
    "queue=subprocess.run(['squeue'":
        "bind(Path(__file__),own=True)\nbind(ROOT/'benchmarks/reports/2026-09-11-direct-fuzz-audit-7394.json',own=True)\nchecked=sum(v['coverage']['checked_verdicts'] for v in legs.values())+10000\nlost=sum(v['coverage']['lost_coverage'] for v in legs.values())\nassert checked+lost==50000\nqueue=subprocess.run(['squeue'",
    "owned_paths=sorted(owned),broad_legs=legs":
        "owned_paths=sorted(owned),status='PASS',generated_cases=50000,checked_generated_verdicts=checked,unverified_timeouts=lost,broad_legs=legs",
    "Checkpoint only. Current-source fuzz requires all four broad legs plus direct lifecycle coverage and terminal cleanup. Do not count timeouts or crash-only corpus checks as verified SAT/UNSAT cases. Production/fixed score unchanged; no denied external transfer/push retried.":
        "Current-source7371/7373 fuzz checkpoint PASS: four broad legs within predeclared coverage bounds and ten thousand direct-root cases. Timeouts and crash-only corpus checks are not verified SAT/UNSAT cases. Not a proof of universal correctness, final integration, memory parity or convergence. Production/fixed score unchanged; no denied external transfer/push retried.",
}
for before,after in changes.items():
    assert code.count(before)==1,before
    code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))
