#!/usr/bin/env python3
"""Seal normal-job integrity and classify only the proved checker-status case."""
from collections import Counter
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
target = root / 'benchmarks/reports/2026-09-08-forward-preconnection-validation.json'
assert not target.exists()
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(8 << 20), b''): h.update(b)
    return h.hexdigest()
classification = root / 'benchmarks/reports/2026-09-08-trivial-checker-status.json'
known = json.loads(classification.read_text())
for rel, digest in known['hashes'].items(): assert sha(root / rel) == digest
hashes = {str(classification.relative_to(root)): sha(classification)}
campaigns = []
for number in (5784, 5811, 5836, 5838, 5855, 5858, 5878, 5880):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): assert sha(c / rel) == digest
    counts = Counter()
    unscored = []
    for p in (c / 'results').glob('*.json'):
        r = json.loads(p.read_text())
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'], p
        assert r['binary_sha256'] == m['arms'][r['arm']]['sha256'], p
        if r['valid']:
            assert r['expected_match'] and r['solve']['exit_code'] in (10, 20), p
            counts['checked_valid'] += 1
        elif r['solve']['timed_out']:
            counts['solver_timeout'] += 1
        else:
            assert number == 5784 and p.stem in ('828', '829'), p
            assert r['instance'] == known['instance'] and r['expected_match'] and r['solve']['exit_code'] == 20
            check = r['proof_check']
            assert check['exit_code'] == 1 and not check['timed_out']
            assert 'c trivial UNSAT\ns VERIFIED' in check['output_tail'] and 'NOT VERIFIED' not in check['output_tail']
            assert m['pin_sha256']['pins/drat-trim'] == known['hashes'][str((c / 'pins/drat-trim').relative_to(root))]
            counts['checker_status_mismatch_unscored'] += 1
            unscored.append(dict(path=str(p.relative_to(root)), sha256=sha(p), arm=r['arm']))
        if number in (5878, 5880): hashes[str(p.relative_to(root))] = sha(p)
    provisional = list((c / 'provisional').glob('*.json'))
    for p in provisional:
        r = json.loads(p.read_text())
        assert r['solve']['timed_out'] or (r['expected_match'] and r['solve']['exit_code'] in (10, 20)), p
    campaigns.append(dict(number=number, counts=dict(counts), provisional=len(provisional), unscored=unscored,
                          manifest_sha256=sha(c / 'manifest.json')))
for number in (5849, 5870):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['files'].items(): assert sha(root / m['source'] / rel) == digest
    for rel, digest in m['binaries'].items(): assert sha(root / rel) == digest
for number in (5869, 5879, 5883):
    c = next((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): assert sha(c / rel) == digest
checks = {}
for name in ('check-doc-size', 'check-section-refs', 'check-option-coverage', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory'):
    r = subprocess.run(['python3', f'tools/docs/{name}.py'], cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (name, r.stdout, r.stderr)
    checks[name] = r.stdout + r.stderr
for label, cmd in [('benchmark-unit-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
                   ('reduce-comparator-tests', ['python3', 'tests/test_compare_reduce_trace.py'])]:
    r = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, (label, r.stdout, r.stderr)
    checks[label] = r.stdout + r.stderr
jobs = '1673402,1673816,1674055,1674132,1674338,1674410,1674668,1674669,1674700,1674781'
queue = subprocess.run(['squeue', '-h', '-j', jobs, '-o', '%i|%P|%T|%C|%m|%R'], capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', '1674504,1674639,1674669,1674781', '-o', 'JobID,State,ExitCode', '-P'], capture_output=True, text=True, check=True).stdout
for rel in ('PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md', 'docs/scheduling-parity-ledger.md',
            'benchmarks/reports/2026-09-08-forward-preconnection-cleanup.md',
            'benchmarks/runs/20260908-5870-forward-preclean/manifest.json',
            'benchmarks/runs/20260908-5869-rank32-reduce-frontier/reduce-validation.json'):
    hashes[rel] = sha(root / rel)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), campaigns=campaigns, checks=checks,
    queue=queue, terminal=terminal, hashes=hashes,
    outcome='5870 tested isolated repair;runtime/causal gates pending.5869 exact Reduce selection match narrows Rank32 next diagnosis. No goal completion or promotion.',
    cleanup='No manual deletion. Completed normal task records assert owned artifact cleanup; active campaigns and uncertain material retained. No fuzz-owner artifacts inspected.',
    checker_disposition='Only exact5784 tasks828/829 matched the previously reproduced input-empty-clause checker status bug;both stay invalid/unscored. Original runner/results unchanged.',
    limitations='Latest-source fuzz remains denied/pending;no retry,bypass,push or production promotion. Pending proofs allow provisional continuation only.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps(campaigns, indent=2))
