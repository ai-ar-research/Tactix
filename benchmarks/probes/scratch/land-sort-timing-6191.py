#!/usr/bin/env python3
"""Land checked local timing and causal-boundary evidence; leave owned jobs live."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-09-sort-timing-landing.json'
assert not target.exists()
assert not subprocess.run(['git', 'diff', '--cached', '--name-only'], cwd=root,
    capture_output=True, text=True, check=True).stdout
owned = set()
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    owned.add(str(p.relative_to(root)))
for name in ('PROGRESS.md', 'benchmarks/reports/2026-09-09-sort-timing-and-rank32.md',
             'benchmarks/reports/2026-09-09-sort-timing-validation.json',
             'benchmarks/reports/2026-09-09-rank32-bve-boundary.md'):
    retain(root / name)
for number in range(6176, 6192):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.py'):
        retain(p)
for p in (root / 'benchmarks/reports/2026-09-09-fresh-rank50-6185').iterdir():
    retain(p)
campaigns = {}; live_jobs = []
for number in (6176, 6180, 6181, 6183, 6188):
    paths = list((root / 'benchmarks/runs').glob(f'20260909-{number}-*'))
    assert len(paths) == 1
    c = paths[0]; m = json.loads((c / 'manifest.json').read_text())
    for key in ('pin_sha256', 'hashes'):
        for rel, digest in m.get(key, {}).items():
            assert sha(c / rel) == digest, rel
            if (c / rel).suffix in ('.py', '.patch'):
                retain(c / rel)
    for name in ('manifest.json', 'launch.json', 'array.sbatch', 'matrix.tsv', 'summary.json', 'solver-checkpoint.json', 'cumulative.patch'):
        if (c / name).is_file(): retain(c / name)
    if (c / 'launch.json').exists():
        launch = json.loads((c / 'launch.json').read_text())
        campaigns[str(c.relative_to(root))] = launch
        if number in (6181, 6188): live_jobs.append(launch['job_id'])
    if number == 6176:
        for wave in range(3): retain(c / f'wave{wave}-submission.json')
        for p in (c / 'results').glob('*.json'): retain(p)
    if number == 6183:
        s = json.loads((c / 'summary.json').read_text())
        for rel, digest in s['hashes'].items():
            assert sha(root / rel) == digest, rel
            if not rel.endswith('.decisions.tsv'): retain(root / rel)
        launch = json.loads((c / 'launch.json').read_text())
        cmd = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
               '-o', 'JobID,State%40,ExitCode,Submit', '-P']
        p = subprocess.run(cmd, capture_output=True, text=True, check=True)
        assert len(p.stdout.strip().splitlines()) == 2 and all('|COMPLETED|0:0|' in s for s in p.stdout.splitlines())
        campaigns[str(c.relative_to(root))]['terminal_accounting'] = dict(command=cmd, stdout=p.stdout)
    if number == 6181:
        s = json.loads((c / 'solver-checkpoint.json').read_text())
        assert s['score']['expected_solves'] == 50 and s['all50_exact_parent_work_and_proof']
        for rel, digest in s['hashes'].items():
            assert sha(root / rel) == digest, rel
            retain(root / rel)
bc = root / 'benchmarks/runs/20260909-6171-learned-sort-BC-gate/solver-checkpoint.json'
retain(bc)
for rel, digest in json.loads(bc.read_text())['hashes'].items():
    assert sha(root / rel) == digest, rel
    retain(root / rel)
a = json.loads((root / 'benchmarks/reports/2026-09-09-sort-timing-validation.json').read_text())
for rel, digest in a['record_hashes'].items():
    assert sha(root / rel) == digest, rel
    retain(root / rel)
for rel, digest in a['protected_campaign_hashes'].items():
    assert sha(root / rel) == digest, rel
checks = {}
for name, command in (
    ('schema-tests', ['python3', 'benchmarks/probes/scratch/test-learned-sort-summary-6182.py']),
    ('benchmark-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
    ('watcher-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/slurm', '-p', 'test_*.py']),
    *[(name, ['python3', 'tools/docs/' + name + '.py']) for name in ('check-doc-size', 'check-progress',
        'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations')]):
    p = subprocess.run(command, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
    print(name, 'PASS', flush=True)
cmd = ['squeue', '-h', '-j', ','.join(live_jobs + ['1677050', '1676920', '1676804']), '-o', '%i %T %M %N']
p = subprocess.run(cmd, capture_output=True, text=True, check=True)
report = root / 'report-to-andrew/20260909-0116-sort-timing'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(hashes={rel: sha(root / rel) for rel in sorted(owned)}, checks=checks,
    campaigns=campaigns, queue=dict(command=cmd, stdout=p.stdout),
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    outcome='Three exact-work Rank4 pairs (six checked proofs) give median0.961681x. All50 candidate expected8403.998s=1.160931951x,38checked at snapshot,all50 parentreplays. Rank32 first laterchoice after78180275matching rows at386968; BVE census6188 runs with required parent replay. Matched41–50 is20checked;fresh accepted99checked.',
    cleanup='No campaign files manually deleted. Generated schema fixture copies auto-cleaned; checked certificates are runner-attested deleted. Large decisionstreams remain local/hash-bound; small terminal prefix/boundary/macro evidence retained. All active sources/pins preserved.',
    next_action='Validate6188 complete parent streams/proofs,then identify first BVE candidate/order/result/effort discrepancy and its producer. Continue full6181,fresh6121 andBC checks without blocking. Memory approval and sourcefuzz gate remain open. No source promotion or push.'), indent=2) + '\n')
retain(target)
subprocess.run(['git', 'add', '-f', '--', *sorted(owned)], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
subprocess.run(['git', 'diff', '--cached', '--stat'], cwd=root, check=True)
subprocess.run(['git', 'commit', '-m', 'Measure learned-sort gain and localize next Rank 32 BVE divergence'], cwd=root, check=True)
assert not subprocess.run(['git', 'status', '--porcelain', '--', *sorted(owned)], cwd=root,
    capture_output=True, text=True, check=True).stdout
print('PASS: local checkpoint landed; no push or production promotion')
