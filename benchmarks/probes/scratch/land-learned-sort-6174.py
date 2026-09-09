#!/usr/bin/env python3
"""Land a coherent isolated experiment checkpoint without pushing or promotion."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-09-learned-sort-landing.json'
assert not target.exists()
assert not subprocess.run(['git', 'diff', '--cached', '--name-only'], cwd=root,
    capture_output=True, text=True, check=True).stdout
owned = set()
def retain(path):
    assert path.is_file() and path.stat().st_size < 5 << 20, path
    owned.add(str(path.relative_to(root)))

for name in ('PROGRESS.md', 'benchmarks/reports/2026-09-09-rank4-learned-sort.md',
             'benchmarks/reports/2026-09-09-learned-sort-validation.json'):
    retain(root / name)
for number in range(6166, 6175):
    for path in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py'):
        retain(path)
for name in ('20260909-6166-rank4-release-profile', '20260909-6171-learned-sort-BC-gate'):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, rel
        if (c / rel).suffix in ('.py', '.patch'):
            retain(c / rel)
    for rel, digest in m['protected_campaign_hashes'].items():
        assert sha(root / rel) == digest, rel
    for rel in ('manifest.json', 'launch.json', 'array.sbatch', 'matrix.tsv'):
        retain(c / rel)
    if '6166-' in name:
        retain(c / 'summary.json')
        for p in (c / 'results').glob('*.json'):
            retain(p)
        for p in (c / 'profiles').glob('*.txt'):
            retain(p)
b = root / 'benchmarks/runs/20260909-6169-learned-sort-build'
m = json.loads((b / 'manifest.json').read_text())
for rel, digest in m['files'].items():
    assert sha(root / m['source'] / rel) == digest, rel
for rel, digest in m['patches'].items():
    assert sha(b / rel) == digest, rel
    retain(b / rel)
for rel, digest in m['logs'].items():
    assert sha(b / rel) == digest, rel
    retain(b / rel)
retain(b / 'manifest.json')
audit = json.loads((root / 'benchmarks/reports/2026-09-09-learned-sort-validation.json').read_text())
for campaign in audit['campaigns']:
    for record in campaign['records']:
        p = root / record['path']
        assert sha(p) == record['sha256'], p
        retain(p)
for path, digest in audit['fresh']['record_hashes'].items():
    p = Path(path)
    assert sha(p) == digest, p
    retain(p)
report = root / 'report-to-andrew/20260909-0056-learned-sort'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (1 page,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
checks = {}
for name, command in (
    ('benchmark-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
    ('watcher-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/slurm', '-p', 'test_*.py']),
    *[(name, ['python3', 'tools/docs/' + name + '.py']) for name in (
        'check-doc-size', 'check-progress', 'check-rule-policy-boundary',
        'check-proof-rule-sync', 'check-section-refs', 'check-citations')]):
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert p.returncode == 0, (name, p.stdout, p.stderr)
    checks[name] = dict(command=command, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)
    print(name, 'PASS', flush=True)
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)},
    checks=checks, andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    outcome='Checked neutral Rank4 Release profiles motivate private constant-byte sort shortcut. Independent102/102 suites and433 cases pass. BC6171 asynchronous;64/66 checked at frozen audit and all32 observed candidate full-work/proof replays exact. No gain or promotion.',
    cleanup='No manual deletion. Published checks assert zero residue; all pending scratch, old campaigns and fixed references retained. Compressed profiles remain local/hash-bound; small text profiles and patches retained.',
    next_action=audit['next_action'], push='Not attempted; existing external push denial remains in force.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Test private learned-sort identity passes after checked Rank 4 profiles'], cwd=root, check=True)
assert not subprocess.run(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root,
    capture_output=True, text=True, check=True).stdout
print('PASS:', len(owned), 'owned paths landed; no push or production promotion', flush=True)
