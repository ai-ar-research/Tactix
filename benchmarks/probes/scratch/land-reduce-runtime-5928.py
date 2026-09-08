#!/usr/bin/env python3
"""Stage only owned immutable evidence; keep active logs and Andrew PDFs local."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-reduce-root-runtime-landing.json'
assert not target.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
report = root / 'report-to-andrew/20260908-1600-reduce-root-runtime'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md', 'docs/scheduling-parity-ledger.md'):
    retain(root / rel)
for name in ('2026-09-08-reduce-root-guidance.md', '2026-09-08-reduce-root-validation.json',
             '2026-09-08-reduce-root-runtime.md', '2026-09-08-reduce-root-runtime-validation.json'):
    retain(root / 'benchmarks/reports' / name)
for number in range(5896, 5929):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'): retain(p)
for number in (5891,5893,5900,5904,5906,5907,5908,5910,5917,5920,5922,5924,5925,5926):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size >= 5 << 20: continue
        if p.suffix not in ('.json','.py','.sbatch','.patch','.cpp','.hpp','.c','.cnf','.proof','.tsv','.log'): continue
        parts = p.relative_to(c).parts
        if number in (5906,5908,5924,5925,5926) and ('logs' in parts or 'traces' in parts): continue
        retain(p)
for number in (5900,5920):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel,digest in m['files'].items(): assert sha(root / m['source'] / rel) == digest
    for rel,digest in m['binaries'].items(): assert sha(root / rel) == digest
    v = json.loads((c / 'final-proof-validation.json').read_text())
    assert v['checked_proofs'] == 46
    for rel,digest in v['hashes'].items(): assert sha(root / rel) == digest
subprocess.run(['git','add','-f','--',*sorted(owned)], cwd=root, check=True)
r = subprocess.run(['git','diff','--cached','--check'], cwd=root, capture_output=True, text=True)
exceptions = []
for line in r.stdout.splitlines():
    match = re.match(r'(.+):(\d+): (.+)', line)
    if not match: continue
    name,num,error = match.groups()
    p = root / name
    value = p.read_text().splitlines()[int(num)-1]
    allowed = (error == 'trailing whitespace.' and
        ((p.suffix == '.patch' and value == ' ') or
         (p.suffix == '.log' and 'clauses in core' in value))) or (
        error == 'new blank line at EOF.' and name in (
            'benchmarks/runs/20260908-5904-reduce-root-edges/forward-fixture.hpp',
            'benchmarks/runs/20260908-5922-reduce-preclean-edges/forward-fixture.hpp'))
    assert allowed, (name,num,error,value)
    exceptions.append(dict(path=name,line=int(num),reason=error,sha256=sha(p)))
target.write_text(json.dumps(dict(hashes={rel:sha(root / rel) for rel in sorted(owned)},
    andrew_report={str((report / name).relative_to(root)):sha(report / name) for name in ('report.tex','report.pdf')},
    frozen_whitespace_exceptions=exceptions,
    limitation='Local investigation checkpoint;no push,promotion or manual deletion. Submitted campaigns and sources immutable. Current-source fuzz remains blocked by denied transfer;no artifacts inspected.5920 gates continue in background.'), indent=2)+'\n')
subprocess.run(['git','add','-f','--',str(target.relative_to(root))],cwd=root,check=True)
print('Staged',len(owned)+1,'owned paths; frozen whitespace exceptions',len(exceptions))
