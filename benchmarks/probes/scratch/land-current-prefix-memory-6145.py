#!/usr/bin/env python3
"""Land the observer repair and sealed evidence, excluding large/live traces."""
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-current-prefix-memory-landing.json'
assert not target.exists()
assert not subprocess.run(['git', 'diff', '--cached', '--name-only'], cwd=root,
    capture_output=True, text=True, check=True).stdout
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
def campaign(number):
    paths = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(paths) == 1, number
    return paths[0]
audit = root / 'benchmarks/reports/2026-09-08-current-prefix-memory-validation.json'
a = json.loads(audit.read_text())
for rel, digest in a['hashes'].items():
    assert sha(root / rel) == digest, rel
for name in ('PROGRESS.md', 'docs/implementation-notes.md',
    'benchmarks/reports/2026-09-08-current-prefix-memory.md',
    'benchmarks/reports/2026-09-08-current-prefix-memory-validation.json'):
    retain(root / name)
for p in (root / 'benchmarks/reports/2026-09-08-fresh-rank50-6143').iterdir():
    retain(p)
for n in range(6135, 6146):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(n) + '.py'):
        retain(p)
for n in (6129, 6135, 6139, 6140):
    c = campaign(n)
    retain(c / 'summary.json')
    summary = json.loads((c / 'summary.json').read_text())
    for rel, digest in summary['hashes'].items():
        assert sha(root / rel) == digest, rel
    if n in (6139, 6140):
        m = json.loads((c / 'manifest.json').read_text())
        for rel, digest in m['hashes'].items():
            assert sha(c / rel) == digest, rel
        for name in ('manifest.json', 'launch.json', 'array.sbatch', 'run.py'):
            retain(c / name)
        for p in (c / 'results').glob('*.json'):
            retain(p)
        for arm in ('candidate', 'kissat'):
            for suffix in ('prefix.tsv', 'boundaries.tsv'):
                retain(c / 'traces' / f'{arm}.{suffix}')
c = campaign(6136)
for name in ('manifest.json', 'build-summary.json', 'repair.patch', 'cumulative.patch'):
    retain(c / name)
for folder in ('logs', 'source'):
    for p in (c / folder).rglob('*'):
        if p.is_file():
            retain(p)
for rel in a['hashes']:
    p = root / rel
    if p.parent.name in ('results', 'provisional') and any(f'-{n}-' in rel for n in (6108, 6114, 6115, 6121, 6129)):
        retain(p)
report = root / 'report-to-andrew/20260908-2345-prefix-memory-checkpoint'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)},
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    queue_at_audit=a['queue'],
    outcome='Three memory probes checked/clean and full-work/proof replay; two bounded current/native decision streams match; unbudgeted Substitute journal repair99/99 twice with old-code-failing test and byte-identical Release. Fresh all50 snapshot100solver/93checked;later audit94checked,BC66/66 andfull6107 48/50.',
    cleanup='No manual deletion,live-log staging or large decision-stream staging. Small sealed prefixes/boundaries retained; large traces remain local/hash-bound. Final normal records assert cleanup; pending jobs and all earlier pins/scripts preserved.',
    next_action='Sample inside BVE around allocation growth and GC,including old/fresh arena overlap,then require full-work/proof replay before attribution. Finish fresh6121 table andpending6108/6115 checks asynchronously. Preserve5900 Rank32 closure andsourcefuzz gate.',
    limitation='No runtime improvement orsourcepromotion. Budgeted Substitute journal transactions remain open. Andrew reportlocal. Previously denied fuzztransfer/push not retried.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Seal Rank 1-50 memory and prefix evidence; restore Substitute journal transaction'], cwd=root, check=True)
assert not subprocess.run(['git', 'status', '--porcelain', '--', *sorted(owned)],
    cwd=root, capture_output=True, text=True, check=True).stdout
print('PASS:', len(owned), 'owned paths landed; no live or large traces,deletion orpush')
