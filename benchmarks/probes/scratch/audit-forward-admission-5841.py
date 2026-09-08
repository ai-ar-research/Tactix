#!/usr/bin/env python3
"""Audit only scoped normal campaigns; never inspect Claude-owned fuzz artifacts."""
from collections import Counter
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-forward-admission-validation.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()
hashes, campaigns = {}, []
def retain(p):
    assert p.is_file() and p.stat().st_size < 5 << 20, p
    hashes[str(p.relative_to(root))] = sha(p)
runtime_numbers = (5758, 5784, 5806, 5811, 5836, 5838)
for number in runtime_numbers:
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1
    c = matches[0]
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, (number, rel)
        if Path(rel).suffix in ('.py', '.patch', '.sbatch', '.tsv'):
            retain(c / rel)
    for p in c.glob('*.json'):
        retain(p)
    counts = Counter()
    for p in (c / 'results').glob('*.json'):
        r = json.loads(p.read_text())
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'], p
        if r['valid']:
            assert r['solve']['exit_code'] in (10, 20) and not r['solve']['timed_out']
            counts['checked_valid'] += 1
        elif r.get('solve', {}).get('timed_out'):
            counts['solver_timeout'] += 1
        else:
            raise RuntimeError(f'Unclassified invalid normal result: {p}')
        retain(p)
    provisional = list((c / 'provisional').glob('*.json'))
    for p in provisional:
        r = json.loads(p.read_text())
        assert r.get('solve', {}).get('timed_out') or (r['expected_match'] and r['solve']['exit_code'] in (10, 20)), p
        retain(p)
    launch_path = c / ('launch.json' if (c / 'launch.json').exists() else 'submission.json')
    launch = json.loads(launch_path.read_text())
    job = launch['job_id'] if 'job_id' in launch else launch['job']
    campaigns.append(dict(campaign=str(c.relative_to(root)), job_id=job,
                          final_counts=dict(counts), provisional_records=len(provisional)))
for number in (5810, 5813, 5815, 5817, 5822, 5825, 5826, 5829, 5830, 5834, 5837):
    matches = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(matches) == 1
    c = matches[0]
    for p in c.rglob('*'):
        if not p.is_file() or p.stat().st_size >= 5 << 20:
            continue
        if p.suffix not in ('.py', '.json', '.log', '.patch', '.sbatch', '.tsv', '.cpp', '.hpp', '.cnf', '.proof'):
            continue
        # All scoped diagnostic solvers above are terminal. Full decision and
        # formula snapshots remain retained on disk but are not staged here.
        retain(p)
    for p in (c / 'results').glob('*.json'):
        r = json.loads(p.read_text())
        assert r['exit_code'] == 0 and r['scratch_deleted'], p
        for rel, digest in r['trace_sha256'].items():
            assert sha(c / 'traces' / rel) == digest, rel
    if (c / 'manifest.json').exists():
        m = json.loads((c / 'manifest.json').read_text())
        for rel, digest in m.get('hashes', {}).items():
            base = root if number == 5817 else c
            assert sha(base / rel) == digest, rel
    assert not list(c.glob('suffix-compare-*')) and not list(c.glob('overlap-test-*'))
for number in (5798, 5830):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['files'].items():
        assert sha(root / m['source'] / rel) == digest, rel
    for rel, digest in m['binaries'].items():
        assert sha(root / rel) == digest, rel
checks = {}
for name in ('check-doc-size', 'check-section-refs', 'check-option-coverage',
             'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory'):
    r = subprocess.run(['python3', f'tools/docs/{name}.py'], cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (name, r.stdout, r.stderr)
    checks[name] = r.stdout + r.stderr
r = subprocess.run(['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py'],
                   cwd=root, capture_output=True, text=True)
assert r.returncode == 0
checks['benchmark-unit-tests'] = r.stdout + r.stderr
jobs = '1673402,1673704,1673816,1673996,1674055,1674056,1674132'
queue = subprocess.run(['squeue', '-h', '-j', jobs, '-o', '%i|%P|%T|%C|%m|%R'], capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1673996,1674056', '-o', 'JobID,State,ExitCode', '-P'],
                          capture_output=True, text=True, check=True).stdout
for number in range(5810, 5842):
    for p in (root / 'benchmarks/probes/scratch').glob('*' + str(number) + '.*'):
        retain(p)
for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md', 'docs/scheduling-parity-ledger.md',
            'tools/bench/decision_prefix.py', 'tools/bench/test_decision_prefix.py',
            'benchmarks/reports/2026-09-08-forward-admission-and-caller-debt.md'):
    retain(root / rel)
report = root / 'report-to-andrew/20260908-1330-forward-admission'
assert 'Overfull' not in (report / 'report.log').read_text()
assert 'Output written on report.pdf (2 pages,' in (report / 'report.log').read_text()
target.write_text(json.dumps(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    campaigns=campaigns, queue=queue, diagnostic_terminal_states=terminal, checks=checks, hashes=hashes,
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    outcome='5830 implemented/tested/unfuzzed;Rank32 first split moved to225987 after61,154,590 common decisions. Full5838 andBC5836 ongoing;goal not met or promoted.',
    cleanup='No manual deletion. Finished real tasks report owned scratch cleanup;temporary comparison-only suffix automatically removed. Small fixed proofs retained as regression evidence. Uncertain/unrelated artifacts untouched.',
    limitations='Pending certificates remain unverified. No Claude-network retry,owner artifact inspection,or push. Generic progress checker sandbox queue warning was not authoritative;explicit live-job query retained here.'), indent=2) + '\n')
print(json.dumps(campaigns, indent=2))
