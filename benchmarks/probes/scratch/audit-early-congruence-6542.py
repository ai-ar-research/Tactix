#!/usr/bin/env python3
"""Scoped integrity/tests/checker accounting; no live scratch or fuzz access."""
import datetime
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from congruence_timing import parse_cumulative

target = root / 'benchmarks/reports/2026-09-09-early-congruence-audit-6542.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-retention-attribution-audit-6533.json'
prior = json.loads(prior_path.read_text())
for group in ('protected_campaign_hashes', 'owned_hashes'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md': assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-09-rank32-early-congruence-timing.md'}
owned.update('tools/bench/' + n + '.py' for n in (
    'runtime_task', 'test_runtime_task', 'congruence_timing', 'test_congruence_timing'))
owned.update('benchmarks/probes/scratch/' + n for n in (
    'prepare-congruence-time-6535.py', 'submit-congruence-time-6536.py',
    'test-wide-padded-xor-6537.cpp', 'check-wide-padded-xor-6538.py',
    'prepare-early-congruence-time-6539.py', 'submit-early-congruence-time-6540.py',
    'summarize-congruence-time-6541.py', 'audit-early-congruence-6542.py',
    'land-early-congruence-6543.py'))
local, campaigns = {}, {}
def read(path, own=False):
    rel = str(path.relative_to(root))
    local[rel] = sha(path)
    if own: owned.add(rel)
    return json.loads(path.read_text())

build = read(root / 'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json')
for rel, digest in build['files'].items(): assert sha(root / build['source'] / rel) == digest, rel
for rel, digest in build['binaries'].items(): assert sha(root / rel) == digest, rel
for name, expected in (
    ('20260908-6108-rank41-50-matched', 20),
    ('20260909-6372-rank32-release-profile', 2),
    ('20260909-6439-activation-ranks1-50', 50),
):
    c = root / 'benchmarks/runs' / name
    rows = [read(p) for p in sorted((c / 'results').glob('*.json'))]
    assert len(rows) == len({r['task'] for r in rows}) == expected
    for r in rows:
        assert classify(r, True)['status'] in ('checked_SAT', 'checked_UNSAT')
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    campaigns[name] = dict(checked_clean=expected)
    print(name, expected, 'checked/clean', flush=True)
fixed = read(root / 'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json')
assert fixed['solver_seconds'] == 7239.010
assert fixed['resource_contract']['cpus_per_task'] == 12
assert fixed['resource_contract']['memory_mib'] == 48000
full = [json.loads(p.read_text()) for p in (root / 'benchmarks/runs/20260909-6439-activation-ranks1-50/results').glob('*.json')]
total = sum(r['solve']['wall_seconds'] for r in full)
assert abs(total - 8731.653) < 1e-8

summary = read(root / 'benchmarks/runs/20260909-6539-rank32-early-congruence-time/summary-6541.json', True)
for rel, digest in summary['hashes'].items():
    assert sha(root / rel) == digest, rel
    local[rel] = digest
assert summary['validation_pending'] and summary['exact_own_checked_work_and_proof']
assert summary['final_cumulative_ms']['index'] == 1969
accounts = dict(prior['accounting'])
for name in ('20260909-6535-rank32-congruence-time', '20260909-6539-rank32-early-congruence-time'):
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json', True)
    launch = read(c / 'launch.json', True)
    assert sha(c / 'manifest.json') == launch['manifest_sha256']
    for rel, digest in m['pin_sha256'].items():
        assert sha(c / rel) == digest, rel
        local[str((c / rel).relative_to(root))] = digest
        if rel.endswith(('.py', '.tsv', '.sbatch')): owned.add(str((c / rel).relative_to(root)))
    p = read(c / 'provisional/000.json', True)
    assert p['validation_pending'] and 'valid' not in p
    if '6539' in name:
        assert parse_cumulative('\n'.join(p['solver_stderr_tail']), 38) == summary['cumulative_rows']
    results = list((c / 'results').glob('*.json'))
    for path in results:
        r = read(path, True)
        assert classify(r, True)['status'] == 'checked_UNSAT'
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    campaigns[name] = dict(solver_complete=True, checked_clean=len(results),
                           independent_validation_pending=not results)
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S',
               launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    accounts[name] = dict(command=command, launch=launch)

accounting = {}
for name, account in accounts.items():
    output = subprocess.check_output(account['command'], text=True)
    assert output.strip(), name
    for line in output.splitlines():
        fields = line.split('|')
        assert fields[1] in ('COMPLETED', 'RUNNING', 'PENDING', 'COMPLETING'), (name, line)
        assert fields[2] == '0:0', (name, line)
    accounting[name] = dict(command=account['command'], stdout=output, launch=account['launch'])
    print(name, 'accounting PASS', flush=True)

c = root / 'benchmarks/runs/20260909-6538-wide-padded-xor-control'
fixture = read(c / 'summary.json', True)
assert fixture['cases'] == 16 and fixture['final_proofs_checked'] == 8 and fixture['removed_bytes'] == 8288
for rel, digest in fixture['hashes'].items(): assert sha(root / rel) == digest, rel
assert sha(c / 'fixture') == fixture['fixture_binary_sha256']
for p in c.iterdir():
    assert not p.is_dir() and p.suffix not in ('.cnf', '.proof'), p
    local[str(p.relative_to(root))] = sha(p)
    if p.suffix in ('.json', '.log'): owned.add(str(p.relative_to(root)))
assert 'error:' in (c / 'build.log').read_text()
assert 'error:' not in (c / 'build-2.log').read_text()

checks = {}
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + n, '-p', 'test_*.py'])
            for n in ('bench', 'slurm')]
commands += [(n, ['python3', 'tools/docs/' + n + '.py']) for n in (
    'check-doc-size', 'check-progress', 'check-rule-policy-boundary',
    'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(command=command, stdout=result.stdout, stderr=result.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1240-early-congruence-timing'
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
local[str((report / 'report-overflow.log').relative_to(root))] = sha(report / 'report-overflow.log')
result = dict(
    observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={r: sha(root / r) for r in sorted(owned)}, local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'],
    campaigns=campaigns, accounting=accounting, checks=checks,
    diagnostic=dict(summary='benchmarks/runs/20260909-6539-rank32-early-congruence-time/summary-6541.json',
                    checked_proof_identity_not_acceptance=True, solver_source_unchanged=True),
    full50=dict(solver_seconds=total, fixed_seconds=7239.010, ratio=total / 7239.010),
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    cleanup='8288 bytes of fixture CNFs/proofs hash-deleted after checks; logs retained. No live scratch or fuzz access. New diagnostic checks remain independent obligations. Prior audit owns unchanged evidence.',
    failure_classification='Before submission, missing manifest checker field required exact-scaffold preparation resume. Fixture field-name compile error retained then corrected before any case. Old6535 provisional omitted stderr; read-only KeyError led to separate6539 capture, not mutation/restart. Initial PDF had0.24336pt overflow, retained then reflowed with two clean compiles. No solver, proof or model failure; no successful writer rerun or denied push/transfer retry.',
    next='Continue Factor source/profile cost attribution before choosing a substantial repair. Deprioritize Congruence and retention tail scans as primary levers. Keep checks asynchronous; no promotion or inferred structural approval. All50/memory goal unmet.',
)
with target.open('x') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
print(target)
