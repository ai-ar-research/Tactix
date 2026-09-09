#!/usr/bin/env python3
"""Land sealed diagnostics and the tested watcher fix; preserve active work."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-09-bve-memory-landing.json'
assert not target.exists()
assert not subprocess.run(['git', 'diff', '--cached', '--name-only'], cwd=root,
    capture_output=True, text=True, check=True).stdout
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
def campaign(number):
    paths = list((root / 'benchmarks/runs').glob(f'2026090[89]-{number}-*'))
    assert len(paths) == 1, number
    return paths[0]
audit = root / 'benchmarks/reports/2026-09-09-bve-memory-validation.json'
a = json.loads(audit.read_text())
for rel, digest in a['hashes'].items():
    assert sha(root / rel) == digest, rel
for name in ('PROGRESS.md', 'tools/slurm/wait.py', 'tools/slurm/test_wait.py',
    'benchmarks/reports/2026-09-09-bve-memory-ownership.md',
    'benchmarks/reports/2026-09-09-bve-memory-validation.json'):
    retain(root / name)
for p in (root / 'benchmarks/reports/2026-09-09-fresh-rank50-6159').iterdir():
    retain(p)
for number in range(6146, 6165):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py'):
        retain(p)
for number in (6146, 6154):
    c = campaign(number)
    for name in ('manifest.json', 'summary.json', 'observer.patch'):
        retain(c / name)
    for folder in ('source', 'logs'):
        for p in (c / folder).rglob('*'):
            if p.is_file(): retain(p)
for number in (6148, 6156):
    c = campaign(number)
    for name in ('manifest.json', 'launch.json', 'matrix.tsv', 'array.sbatch', 'summary.json'):
        retain(c / name)
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, rel
        if (c / rel).suffix in ('.py', '.patch'): retain(c / rel)
    for rel, digest in json.loads((c / 'summary.json').read_text())['hashes'].items():
        assert sha(root / rel) == digest, rel
    # Only final hash-bound memory traces; no live solver/checker logs.
    for p in (c / 'logs').glob('*.slot-memory.tsv'):
        retain(p)
c = campaign(6160)
for name in ('manifest.json', 'launch.json', 'array.sbatch', 'run.py', 'summary.json'):
    retain(c / name)
for rel, digest in json.loads((c / 'summary.json').read_text())['hashes'].items():
    assert sha(root / rel) == digest, rel
for p in (c / 'results').glob('*.json'):
    retain(p)
for arm in ('candidate', 'kissat'):
    for suffix in ('prefix.tsv', 'boundaries.tsv'):
        retain(c / 'traces' / f'{arm}.{suffix}')
for rel in a['hashes']:
    p = root / rel
    if p.parent.name in ('results', 'provisional') and any(f'-{n}-' in rel for n in (6108, 6114, 6115, 6121, 6129, 6148, 6156)):
        retain(p)
for item in a['campaigns']:
    if item['number'] == 5784:
        for row in item['unscored']:
            p = root / row['path']
            assert sha(p) == row['sha256']
            retain(p)
report = root / 'report-to-andrew/20260909-0028-bve-memory-checkpoint'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)},
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    queue_at_audit=a['queue'],
    outcome='6148/6156 each3checked memory witnesses; full work/proof/capacity replay. Dense growth/compaction owns next experimental question,not GC overlap. Rank4 907045 decisions and380searchboundaries equal through650K. Latestobserver101Release/100trace,byteidenticalRelease. Watcher13tests andbenchmark39tests pass; recycledID verified withsubmissiontime. Freshall50 100solver/96checked,full6115 49checked,BC66checked.',
    cleanup='Onlytwozero-byte intentional-regression tempfiles removed,exactpaths inbuildmanifests; no certificate/livepin/uniqueevidence deletion. Small terminal hash-bound memory traces retained; large decisionstreams remain local/hash-bound. Existingcampaignscripts andfixedreferences untouched.',
    next_action='Obtain explicit revamp approval for private FlatLists realloc buffer; no structuraledit yet. Growth-only change doesnot establish rank21 compaction repair. Continue fresh6121/6108/6115 proofs andrank4 unit-cost profile; preserveRank32/N1-N4 andsourcefuzz gate.',
    limitation='No production solver change,runtime gain orpromotion. Reportslocal. Deniedfuzztransfer/push not retried. PROGRESS solvergate count48 is its preceding snapshot;thisaudit has49/50.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Localize BVE memory peaks and harden SLURM completion identity'], cwd=root, check=True)
assert not subprocess.run(['git', 'status', '--porcelain', '--', *sorted(owned)],
    cwd=root, capture_output=True, text=True, check=True).stdout
print('PASS:', len(owned), 'owned paths landed; no push or active-campaign mutation')
