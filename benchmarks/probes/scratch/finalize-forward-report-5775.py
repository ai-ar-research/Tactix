#!/usr/bin/env python3
"""Create a corrected final report without changing the frozen checkpoint report."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
old=root / 'report-to-andrew/20260908-1055-forward-unit-exit'
c=root / 'report-to-andrew/20260908-1105-forward-exit-final'
c.mkdir(exist_ok=False)
text=(old / 'report.tex').read_text()
before='''Claude owner 5765/session 8493 runs the new 5741 fuzz checkpoint in the
background and owns the jobs, oracle checks and artifact interpretation. Only its CLI
response is polled. Prior 5715 is terminal PASS for 5672, not current coverage.'''
after='''Claude owner 5765 returned only an interim waiting message through CLI
8493, not a terminal verdict. Helper 5774 resumes the same Claude session;
the new live CLI handle is 38524. No duplicate campaign is authorized. Only
that response is polled; the owner retains all job and oracle interpretation.
Prior 5715 is terminal PASS for 5672, not current coverage.'''
assert before in text
(c / 'report.tex').write_text(text.replace(before,after))
for i in range(2):
    result=subprocess.run(['bash','-lc','module load amh-tex/2023 && pdflatex -interaction=nonstopmode -halt-on-error report.tex'],
        cwd=c,capture_output=True,text=True,check=True)
    (c / f'compile-{i}.log').write_text(result.stdout+result.stderr)
log=(c / 'report.log').read_text()
assert 'Output written on report.pdf' in log and 'Overfull' not in log
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
paths=['PROGRESS.md','benchmarks/reports/2026-09-08-forward-exit-owner-followup.md',
       'benchmarks/reports/2026-09-08-forward-unit-exit-validation.json',
       str((c / 'report.tex').relative_to(root)),str((c / 'report.pdf').relative_to(root)),
       'benchmarks/probes/scratch/resume-fuzz-owner-5774.py']
checks={}
for name in ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary'):
    checks[name]=subprocess.run(['python3',root / 'tools/docs' / (name+'.py')],cwd=root,capture_output=True,text=True,check=True).stdout
target=root / 'benchmarks/reports/2026-09-08-forward-exit-owner-followup-validation.json'
assert not target.exists()
target.write_text(json.dumps(dict(hashes={p:sha(root / p) for p in paths},checks=checks,
    andrew_compilations=2,owner_session='3c676397-a271-4992-bc38-a4d2786e8724',live_cli=38524,
    correction='8493 returned a wait-only response,not PASS. Same owner resumed;no duplicate fuzz campaign or independent artifact inspection.',
    test_consideration='Owner-lifecycle/documentation handoff only;solver,harness,oracle and acceptance conditions unchanged. Existing10 scoring tests and91 trace suite remain authoritative;no new solver unit test needed.'),indent=2)+'\n')
print(c / 'report.pdf')
