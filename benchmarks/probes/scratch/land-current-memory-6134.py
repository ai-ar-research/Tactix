#!/usr/bin/env python3
"""Land tested observer plumbing and immutable checkpoint evidence only."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-current-memory-landing.json'
assert not target.exists()
assert not subprocess.run(['git', 'diff', '--cached', '--name-only'], cwd=root,
    capture_output=True, text=True, check=True).stdout
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
audit = root / 'benchmarks/reports/2026-09-08-current-memory-validation.json'
a = json.loads(audit.read_text())
for rel, digest in a['hashes'].items():
    assert sha(root / rel) == digest, rel
for name in ('PROGRESS.md', 'tools/bench/slot_memory_task.py', 'tools/bench/test_slot_memory_task.py',
    'benchmarks/reports/2026-09-08-current-memory-diagnostics.md',
    'benchmarks/reports/2026-09-08-current-memory-validation.json'):
    retain(root / name)
for p in (root / 'benchmarks/reports/2026-09-08-fresh-rank50-6132').iterdir():
    retain(p)
for n in range(6127, 6135):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.py'):
        retain(p)
c = root / 'benchmarks/runs/20260908-6127-publication-diagnostic-builds'
retain(c / 'summary.json')
for p in (c / 'logs').iterdir():
    # Build and fixed-test logs are terminal, unlike live solver traces.
    retain(p)
c = root / 'benchmarks/runs/20260908-6129-publication-memory'
for name in ('manifest.json', 'launch.json', 'matrix.tsv', 'array.sbatch'):
    retain(c / name)
for p in (c / 'pins').glob('*.py'):
    retain(p)
for rel in a['hashes']:
    p = root / rel
    if p.parent.name in ('results', 'provisional') and any(f'-{n}-' in rel for n in (6108, 6114, 6115, 6121, 6129)):
        retain(p)
report = root / 'report-to-andrew/20260908-2305-memory-ownership-checkpoint'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)},
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    queue_at_audit=a['queue'],
    outcome='Unchanged6107 trace98/98,journal99/99;39benchmarktests;memoryprobes1676999 submitted;freshall50table updated withoutpartialaggregate.',
    cleanup='No manual deletion or live log/trace staging. Final normal records assert cleanup; pending jobs and prior pins/scripts preserved.',
    next_action='Run6131 after1676999 terminalrecords;requirefullReleasework/proof replay beforememoryattribution. Finish6121classification/all50table;retainpending6108/6114/6115checks andsourcefuzz gate.',
    limitation='No solver sourcepromotion orperformanceacceptance. Reportslocal. Previouslydenied fuzztransfer/pushnotretried.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Pin current-source memory diagnostics and preserve matched Rank 1-50 evidence'], cwd=root, check=True)
assert not subprocess.run(['git', 'status', '--porcelain', '--', *sorted(owned)],
    cwd=root, capture_output=True, text=True, check=True).stdout
print('PASS:', len(owned), 'owned paths landed; no live traces, deletion or push')
