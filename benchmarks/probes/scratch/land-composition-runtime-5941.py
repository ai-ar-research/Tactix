#!/usr/bin/env python3
"""Land the rejected composition and earlier formula evidence, without mutation."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-composition-runtime-landing.json'
assert not target.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
report=root/'report-to-andrew/20260908-1625-composition-runtime'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size<5<<20,p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md','docs/scheduling-parity-ledger.md',
    'benchmarks/reports/2026-09-08-composition-runtime-and-formula.md',
    'benchmarks/reports/2026-09-08-composition-runtime-validation.json',
    'benchmarks/reports/2026-09-08-trivial-checker-status-second.json'):
    retain(root/rel)
for number in range(5931,5943):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(number)+'.*'): retain(p)
for number in (5924,5926,5931,5932,5937):
    c=next((root/'benchmarks/runs').glob(f'20260908-{number}-*'))
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size>=5<<20: continue
        if 'logs' in p.relative_to(c).parts or 'traces' in p.relative_to(c).parts: continue
        if number==5937 and p.name in ('summary.json','bve-frontier.json'): continue
        if p.suffix in ('.json','.py','.sbatch','.tsv'): retain(p)
for number in (1280,1281):
    retain(root/f'benchmarks/runs/20260908-5784-experimental-tierd-paired-cpuq-4cpu-32g-1200s/results/{number}.json')
checks={}
for name in ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary','check-proof-rule-sync','check-inventory'):
    r=subprocess.run(['python3',f'tools/docs/{name}.py'],cwd=root,capture_output=True,text=True)
    assert r.returncode==0,(name,r.stdout,r.stderr)
    checks[name]=r.stdout+r.stderr
target.write_text(json.dumps(dict(hashes={rel:sha(root/rel) for rel in sorted(owned)},checks=checks,
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    limitation='Local checkpoint,no push,promotion or manual deletion. Retain5900;5920 rejected atfull/C.5937 terminal records retained butcomparison running separately,not claimed validated. Four exact checker-status false negatives remain unscored underimmutablecpu-q runner.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
print('PASS:',len(owned),'owned evidence paths staged;large raw snapshots remain retained locally')
