#!/usr/bin/env python3
"""Audit scoped normal campaigns and landing evidence, excluding Claude fuzz."""
from collections import Counter
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-forward-caller-validation.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
hashes = {}
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    hashes[str(p.relative_to(root))] = sha(p)
campaigns = []
for number in [5784, 5811, 5836, 5838, 5855, 5858]:
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest
        if number in (5855, 5858) and (c / rel).suffix in ['.py', '.patch', '.tsv', '.sbatch']:
            retain(c / rel)
    counts = Counter()
    for p in (c / 'results').glob('*.json'):
        r = json.loads(p.read_text())
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'], p
        if r['valid']: counts['checked_valid'] += 1
        elif r['solve']['timed_out']: counts['solver_timeout'] += 1
        else: raise AssertionError(('invalid normal result', p))
        if number in (5855, 5858): retain(p)
    provisional = list((c / 'provisional').glob('*.json'))
    for p in provisional:
        r = json.loads(p.read_text())
        assert r['solve']['timed_out'] or (r['expected_match'] and r['solve']['exit_code'] in (10, 20)), p
        if number in (5855, 5858): retain(p)
    if number in (5855, 5858):
        for p in c.glob('*.json'): retain(p)
    campaigns.append(dict(number=number, counts=dict(counts), provisional=len(provisional),
                          manifest_sha256=sha(c / 'manifest.json')))
for number in [5845, 5849, 5853, 5856, 5857, 5859, 5862]:
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    if (c / 'manifest.json').exists():
        m = json.loads((c / 'manifest.json').read_text())
        for rel, digest in m.get('hashes', {}).items(): assert sha(c / rel) == digest
    if number != 5862:
        for p in (c / 'results').glob('*.json'):
            r = json.loads(p.read_text())
            assert r['exit_code'] == 0 and r['scratch_deleted'], p
            for rel, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / rel) == digest
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size >= 5 << 20: continue
        if number == 5862 and (p.parent.name in ('logs', 'traces', 'results')): continue
        if p.suffix in ['.py', '.json', '.log', '.patch', '.sbatch', '.tsv', '.cpp', '.hpp', '.cnf', '.proof']:
            retain(p)
    assert not list(c.glob('suffix-compare-*')) and not list(c.glob('overlap-test-*'))
for number in [5830, 5849]:
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['files'].items(): assert sha(root / m['source'] / rel) == digest
    for rel, digest in m['binaries'].items(): assert sha(root / rel) == digest
checks = {}
for name in ['check-doc-size', 'check-section-refs', 'check-option-coverage',
             'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory']:
    run = subprocess.run(['python3', f'tools/docs/{name}.py'], cwd=root,
                         capture_output=True, text=True)
    assert run.returncode == 0, (name, run.stdout, run.stderr)
    checks[name] = run.stdout + run.stderr
run = subprocess.run(['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py'],
                     cwd=root, capture_output=True, text=True)
assert run.returncode == 0
checks['benchmark-unit-tests'] = run.stdout + run.stderr
jobs = '1673402,1673816,1674055,1674132,1674338,1674339,1674340,1674410,1674504'
queue = subprocess.run(['squeue', '-h', '-j', jobs, '-o', '%i|%P|%T|%C|%m|%R'],
                       capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1674241,1674339,1674340', '-o', 'JobID,State,ExitCode', '-P'],
                          capture_output=True, text=True, check=True).stdout
for number in range(5848, 5866):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'): retain(p)
for rel in ['PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md', 'docs/scheduling-parity-ledger.md',
            'benchmarks/reports/2026-09-08-forward-caller-closure.md',
            'benchmarks/reports/2026-09-08-forward-caller-checkpoint.json']:
    retain(root / rel)
report = root / 'report-to-andrew/20260908-1421-forward-caller-closure'
log = (report / 'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
target.write_text(json.dumps(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    campaigns=campaigns, queue=queue, terminal=terminal, checks=checks, hashes=hashes,
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ['report.tex', 'report.pdf']},
    outcome='5849 tested/unfuzzed;Rank31 caller count residual closed,31/32 decision trajectories unchanged through300K. BC solver screen improves both tiers;full40 incomplete. Next BVE frontier diagnostic1674504 running.',
    test_consideration='New solver behaviors have focused state/model/proof/journal tests. Comparator/runner adapters retain existing protocols and hashes;22 maintained benchmark tests and6 docs checks pass. No new checker acceptance logic.',
    cleanup='No manual deletion. Terminal real tasks report owned scratch cleanup;comparison temporary suffix was automatically removed. Small fixed proofs retained. Initial mocked snapshot cleanup residue left untouched rather than inspected as a live fuzz artifact.',
    limitations='No promotion,goal completion,push,Claude-network retry or fuzz-owner artifact inspection. Pending certificates remain unverified.'), indent=2) + '\n')
print(json.dumps(campaigns, indent=2))
