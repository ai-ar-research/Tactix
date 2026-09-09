#!/usr/bin/env python3
"""Validate owned diagnostics and record hygiene without mutating any campaign."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-09-expanded-ranks-audit-6204.json'
assert not target.exists()
owned = set(); large = {}; campaigns = {}
def retain(p):
    assert p.is_file(), p
    rel = str(p.relative_to(root))
    if p.stat().st_size >= 5 << 20 or rel.endswith('.decisions.tsv'):
        large[rel] = dict(bytes=p.stat().st_size, sha256=sha(p))
    else: owned.add(rel)
for name in ('PROGRESS.md', 'benchmarks/reports/2026-09-09-expanded-ranks-and-bve-order.md',
             'benchmarks/reports/2026-09-09-full50-certificates-6197.json',
             'benchmarks/reports/2026-09-09-rank50-loss-breakdown-6198.json'):
    retain(root / name)
for number in range(6192, 6206):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py'): retain(p)
for name in ('20260909-6188-rank32-bve-candidates', '20260909-6194-rank32-selected-heaps',
             '20260909-6200-rank50-current-prefix', '20260909-6201-rank49-current-prefix'):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest, rel
    s = json.loads((c / 'summary.json').read_text())
    if 'rank32' in name: assert s['parent_replay']
    for rel, digest in s['hashes'].items():
        assert sha(root / rel) == digest, rel
        retain(root / rel)
    for filename in ('manifest.json', 'launch.json', 'run.py', 'array.sbatch', 'summary.json', 'attempt-alignment-6193.json'):
        if (c / filename).exists(): retain(c / filename)
    for arm in ('candidate', 'kissat'):
        p = c / 'results' / (arm + '.json'); r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted']
        assert r['partial_certificate']['bytes'] > 0
        assert r['binary_sha256'] == sha(c / 'pins' / arm)
        retain(p)
    campaigns[str(c.relative_to(root))] = json.loads((c / 'launch.json').read_text())
sealed = json.loads((root / 'benchmarks/reports/2026-09-09-full50-certificates-6197.json').read_text())
for rel, digest in sealed['hashes'].items():
    assert sha(root / rel) == digest, rel
    retain(root / rel)
previous = json.loads((root / 'benchmarks/reports/2026-09-09-sort-timing-validation.json').read_text())
for rel, digest in previous['protected_campaign_hashes'].items(): assert sha(root / rel) == digest, rel
checks = {}
commands = [(f'fixtures-{number}', ['python3', 'benchmarks/probes/scratch/' + name, '--self-test']) for number, name in (
    (6192, 'validate-rank32-bve-candidates-6192.py'), (6193, 'align-rank32-bve-attempts-6193.py'),
    (6196, 'validate-rank32-selected-heaps-6196.py'))]
commands += [('benchmark-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
             ('watcher-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/slurm', '-p', 'test_*.py'])]
commands += [(name, ['python3', 'tools/docs/' + name + '.py']) for name in ('check-doc-size', 'check-progress',
    'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    p = subprocess.run(command, capture_output=True, text=True, check=True, cwd=root)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-0143-expanded-ranks'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={rel: sha(root / rel) for rel in sorted(owned)},
    large_local_hash_bound=large, campaigns=campaigns, checks=checks,
    protected_campaign_hashes=previous['protected_campaign_hashes'],
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    cleanup='No manual campaign deletion. All eight diagnostic tasks attest partial proof and scratch deletion. All final accepted certificate records in6197 pass strict checker/cleanup classification. Live checker scratch is owned by its runner; no cleanup attempted. Large raw traces retained locally/hash-bound.',
    scope='No solver source changes, promotion, denied fuzz transfer, fuzz artifact inspection, or push. Full50 target unmet; next membership producer and added-rank causal investigation are useful work.'), indent=2) + '\n')
print(target)
