#!/usr/bin/env python3
"""Stage only owned checkpoint evidence and verify the delivered report."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-forward-preconnection-landing.json'
assert not target.exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
report = root / 'report-to-andrew/20260908-1512-forward-preconnection-cleanup'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md', 'docs/scheduling-parity-ledger.md'):
    retain(root / rel)
for name in ('2026-09-08-forward-preconnection-cleanup.md', '2026-09-08-forward-preconnection-validation.json', '2026-09-08-trivial-checker-status.json'):
    retain(root / 'benchmarks/reports' / name)
for number in range(5868, 5890):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'): retain(p)
for p in (root / 'benchmarks/probes/scratch/5874-checker-status').iterdir(): retain(p)
for number in (5862, 5869, 5870, 5875, 5876, 5878, 5879, 5880, 5883):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    # Existing failed5862 raw captures remain untouched; retain its new diagnosis only.
    if number == 5862:
        retain(c / 'schedule-diagnosis.json')
        continue
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size >= 5 << 20: continue
        if p.suffix in ('.json', '.py', '.sbatch', '.patch', '.cpp', '.hpp', '.c', '.cnf', '.proof', '.tsv', '.log'):
            # Running task logs are mutable and never landing authority.
            if 'logs' in p.relative_to(c).parts and number in (5878, 5880, 5883): continue
            if 'traces' in p.relative_to(c).parts and number == 5883: continue
            retain(p)
checks = {}
for name in ('check-doc-size', 'check-section-refs', 'check-option-coverage', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory'):
    r = subprocess.run(['python3', f'tools/docs/{name}.py'], cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (name, r.stdout, r.stderr)
    checks[name] = r.stdout + r.stderr
manifest = root / 'benchmarks/runs/20260908-5870-forward-preclean/manifest.json'
m = json.loads(manifest.read_text())
for rel, digest in m['files'].items(): assert sha(root / m['source'] / rel) == digest
for rel, digest in m['binaries'].items(): assert sha(root / rel) == digest
out = dict(checks=checks, hashes={rel: sha(root / rel) for rel in sorted(owned)},
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    limitation='Local investigation checkpoint only;production unchanged,current-source fuzz blocked,gates not accepted. No push or deletion. Older audit binds its own earlier status/docs snapshot;this record binds landing state.')
target.write_text(json.dumps(out, indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
print('Staged', len(owned), 'owned evidence paths;Andrew report remains local/untracked')
