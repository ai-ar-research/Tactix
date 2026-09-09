#!/usr/bin/env python3
"""Audit named normal campaigns and this diagnostic-only increment; never fuzz."""
from collections import Counter
import datetime as dt
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha
target = root / 'benchmarks/reports/2026-09-08-current-memory-validation.json'
assert not target.exists()
hashes = {}
def bind(p):
    hashes[str(p.relative_to(root))] = sha(p)
def campaign(number):
    paths = list((root / 'benchmarks/runs').glob(f'20260908-{number}-*'))
    assert len(paths) == 1, number
    return paths[0]

# The broad cpu-q campaign includes UNKNOWN ground truth, unlike the fixed50
# classifier's known-verdict contract. Retain its established timeout/status
# classification rather than silently assigning an expected verdict.
prior = root / 'benchmarks/reports/2026-09-08-fresh-accepted-validation.json'
old = json.loads(prior.read_text())
known_status = {r['path']: r['sha256'] for c in old['campaigns']
                for r in c['unscored'] if r.get('reason') != 'checker_timeout'}
assert len(known_status) == 4
bind(prior)
spec = importlib.util.spec_from_file_location('checker_timeouts',
    Path(__file__).with_name('classify-checker-timeout-5981.py'))
timeouts = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timeouts)
timeouts.fixtures()

def cpuq_classification(r, path, allowance):
    clean = r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    if r['valid']:
        assert r['verdict'] in ('SAT', 'UNSAT') and r['expected_match'], path
        assert r['expected'] in ('UNKNOWN', r['verdict']), path
        assert not r['solve']['timed_out'], path
        assert r['solve']['exit_code'] == (10 if r['verdict'] == 'SAT' else 20), path
        check = r['model_check' if r['verdict'] == 'SAT' else 'proof_check']
        assert check['exit_code'] == 0 and not check['timed_out'] and check['result'] == 'verified', path
        if r['verdict'] == 'UNSAT':
            assert 's VERIFIED' in check['output_tail'] and 's NOT VERIFIED' not in check['output_tail'], path
        return dict(status='checked_' + r['verdict'], clean=clean, checked=True)
    if r['solve']['timed_out']:
        assert r['verdict'] == 'UNKNOWN' and not r['expected_match'], path
        return dict(status='solver_timeout', clean=clean, checked=False)
    if timeouts.checker_timeout(r, allowance):
        return dict(status='checker_timeout', clean=clean, checked=False)
    rel = str(path.relative_to(root))
    assert rel in known_status and sha(path) == known_status[rel], path
    return dict(status='frozen_checker_status_case', clean=clean, checked=False)

campaigns = []
jobs = []
for number in (5784, 6108, 6114, 6115, 6121, 6129):
    c = campaign(number)
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, (number, rel)
    for rel, digest in m.get('protected_campaign_hashes', {}).items():
        assert sha(root / rel) == digest, rel
    for name in ('manifest.json', 'launch.json', 'matrix.tsv', 'array.sbatch'):
        bind(c / name)
    launch = json.loads((c / 'launch.json').read_text())
    job = str(launch['job_id'])
    assert job.isdigit(), launch
    jobs.append(job)
    classes = Counter()
    unscored = []
    records = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text())
            task = r['task']
            assert task == int(p.stem), p
            if folder == 'results':
                kind = (cpuq_classification(r, p, m['resource_contract']['checker_timeout'])
                        if number == 5784 else classify(r, True))
                assert kind['clean'], p
                assert r['binary_sha256'] == m['arms'][r['arm']]['sha256'], p
                if task in records:
                    for key in ('solve', 'certificate_sha256', 'certificate_bytes'):
                        assert records[task][key] == r[key], (p, key)
                classes[kind['status']] += 1
                if not kind['checked'] and kind['status'] != 'solver_timeout':
                    unscored.append(dict(path=str(p.relative_to(root)), sha256=sha(p), classification=kind['status']))
            records[task] = r
            if number != 5784:
                bind(p)
    campaigns.append(dict(number=number, job=job, solver_records=len(records),
                          final_classes=dict(classes), unscored=unscored))

build = campaign(6127)
b = json.loads((build / 'summary.json').read_text())
assert sha(root / b['authority'] / 'manifest.json') == b['authority_sha256']
for rel, digest in b['source_sha256'].items():
    assert sha(root / b['source'] / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel / 'tactix') == digest, rel
for rel, digest in b['logs'].items():
    assert sha(build / rel) == digest, rel
    bind(build / rel)
bind(build / 'summary.json')
for kind, count in (('trace', 98), ('journal', 99)):
    assert f'100% tests passed out of {count}' in (build / 'logs' / f'{kind}-ctest.log').read_text()
for name in ('rank1-40-kissat-12cpu-v1', 'rank1-50-kissat-12cpu-v1'):
    bind(root / 'benchmarks/baselines' / name / 'manifest.json')
snapshot = root / 'benchmarks/reports/2026-09-08-fresh-rank50-6132'
s = json.loads((snapshot / 'summary.json').read_text())
assert s['ranks'] == 50 and len(s['pairs']) == 50 and s['ratio'] is None
for rel, digest in s['record_hashes'].items():
    assert sha(Path(rel)) == digest, rel
for p in snapshot.iterdir():
    bind(p)
checks = {}
commands = [(name, ['python3', f'tools/docs/{name}.py']) for name in
    ('check-doc-size', 'check-section-refs', 'check-option-coverage',
     'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-inventory')]
commands += [('benchmark-unit-tests', ['python3', '-m', 'unittest', 'discover', '-s', 'tools/bench', '-p', 'test_*.py']),
    ('memory-seal-fixtures', ['python3', 'benchmarks/probes/scratch/seal-publication-memory-6131.py', '--self-test'])]
for name, command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert p.returncode == 0, (name, p.stdout, p.stderr)
    checks[name] = p.stdout + p.stderr
queue = subprocess.run(['squeue', '-h', '-j', ','.join(jobs), '-o', '%i|%P|%T|%C|%m|%R'],
    capture_output=True, text=True, check=True).stdout
terminal = subprocess.run(['sacct', '-n', '-X', '-j', ','.join(jobs[1:]), '-o', 'JobID,State,ExitCode', '-P'],
    capture_output=True, text=True, check=True).stdout
for rel in ('PROGRESS.md', 'tools/bench/slot_memory_task.py', 'tools/bench/test_slot_memory_task.py',
            'benchmarks/reports/2026-09-08-current-memory-diagnostics.md'):
    bind(root / rel)
target.write_text(json.dumps(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    hashes=hashes, campaigns=campaigns, checks=checks, queue=queue, terminal=terminal,
    outcome='Unchanged6107 trace98/98 andjournal99/99;39benchmarktests andsixnegativeparserfixtures. Memory6129/job1676999 queued;noownershiporperformanceclaim. Fresh6132all50retained,74solves58checked,noaggregate.',
    cleanup='No manual deletion. All published final normal records assert original-certificate and derivative cleanup; active scratch,pins andlogs untouched. No fuzz-owner artifacts accessed.',
    limitation='Normalpendingchecks continue;knowncpuq checker failuresremainunscored. Firstauditattempt correctlyrejected UNKNOWN expectedverdict frombroadcpuq;retained itsestablishedtimeout/statusclassifier instead ofassigninggroundtruth orweakeningfixed50classification. No sourcepromotion,reference replacement,fuzztransferretry orpushretry.'), indent=2) + '\n')
print(json.dumps(campaigns, indent=2))
