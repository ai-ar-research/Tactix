#!/usr/bin/env python3
"""Record the negative causal result and its bounded, validated handoff."""
import hashlib
import json
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5925-reduce-preclean-rank32-prefix'
report = root / 'report-to-andrew/20260908-1605-composition-prefix'
target = root / 'benchmarks/reports/2026-09-08-composition-prefix-validation.json'
assert not target.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (' in log
checks = {}
for name in ('check-doc-size','check-section-refs','check-option-coverage',
             'check-rule-policy-boundary','check-proof-rule-sync','check-inventory'):
    r = subprocess.run(['python3',f'tools/docs/{name}.py'],cwd=root,capture_output=True,text=True)
    assert r.returncode == 0,(name,r.stdout,r.stderr)
    checks[name] = r.stdout + r.stderr
owned = [root / 'PROGRESS.md',root / 'benchmarks/reports/2026-09-08-composition-prefix.md',
    Path(__file__),Path(__file__).with_name('seal-composition-prefix-5929.py'),
    c / 'summary.json',c / 'parent-replay.json',c / 'results/candidate.json']
m = json.loads((c / 'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c / rel) == digest
target.write_text(json.dumps(dict(checks=checks,hashes={str(p.relative_to(root)):sha(p) for p in owned},
    andrew_report={str((report / name).relative_to(root)):sha(report / name) for name in ('report.tex','report.pdf')},
    limitation='Negative local causal result,not full performance acceptance. Full5926 andBC5924 continue. Older reports,launches,pins andsnapshots remain frozen;no deletion,push,promotion or fuzz-artifact inspection.'),indent=2)+'\n')
subprocess.run(['git','add','-f','--',*[str(p.relative_to(root)) for p in [*owned,target]]],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
print('PASS: negative result,report anddocs validated; eight explicit paths staged')
