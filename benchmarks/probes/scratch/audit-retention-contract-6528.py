#!/usr/bin/env python3
"""Read-only campaign audit plus fixed baseline tests; write one new checkpoint."""
import datetime
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root / 'benchmarks/reports/2026-09-09-retention-contract-audit-6528.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-forward-boundaries-audit-6519.json'
prior = json.loads(prior_path.read_text())
for group in ('protected_campaign_hashes', 'owned_hashes'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {
    'PROGRESS.md', 'benchmarks/reports/2026-09-09-rank32-retention-contract.md',
    'benchmarks/probes/scratch/test-binary-retention-contract-6527.cpp',
    'benchmarks/probes/scratch/audit-retention-contract-6528.py',
    'benchmarks/probes/scratch/land-retention-contract-6529.py',
}
local = {}
build = json.loads((root / 'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json').read_text())
for rel, digest in build['files'].items():
    assert sha(root / build['source'] / rel) == digest, rel
for rel, digest in build['binaries'].items():
    assert sha(root / rel) == digest, rel
for rel in (
    build['source'] + '/src/kernel/watch.hpp',
    build['source'] + '/src/kernel/kernel.cpp',
    'tools/solvers/variants/kissat-macro-heap-6090/src/watch.c',
    'tools/solvers/variants/cadical-extdiag/src/watch.cpp',
):
    local[rel] = sha(root / rel)

campaigns = {}
for name, expected in (
    ('20260908-6108-rank41-50-matched', 20),
    ('20260909-6372-rank32-release-profile', 2),
    ('20260909-6439-activation-ranks1-50', 50),
):
    c = root / 'benchmarks/runs' / name
    rows = []
    for p in sorted((c / 'results').glob('*.json')):
        row = json.loads(p.read_text())
        status = classify(row, True)
        assert status['status'] in ('checked_SAT', 'checked_UNSAT'), (p, status)
        assert row['artifact_deleted'] and row['derived_artifacts_deleted']
        assert not row['cleanup_pending']
        local[str(p.relative_to(root))] = sha(p)
        rows.append(row)
    assert len(rows) == expected
    assert len({row['task'] for row in rows}) == expected
    campaigns[name] = dict(checked_clean=expected, verdicts={
        v: sum(r['verdict'] == v for r in rows) for v in ('SAT', 'UNSAT')})
    print(name, expected, 'checked/clean', flush=True)

def read(rel):
    p = root / rel
    local[rel] = sha(p)
    return json.loads(p.read_text())

profile = read('benchmarks/runs/20260909-6372-rank32-release-profile/results/000.json')
native = read('benchmarks/runs/20260909-6372-rank32-release-profile/results/001.json')
current = read('benchmarks/runs/20260909-6439-activation-ranks1-50/results/031.json')
nt = lambda r: [s for s in r['solver_statistics'] if not s.startswith('c timing')]
assert nt(profile) == nt(current)
for key in ('instance', 'rank', 'certificate_sha256', 'certificate_bytes', 'verdict'):
    assert profile[key] == current[key], key
fixed = read('benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json')
assert fixed['solver_seconds'] == 7239.010
assert fixed['resource_contract']['cpus_per_task'] == 12
assert fixed['resource_contract']['memory_mib'] == 48000
full = [json.loads(p.read_text()) for p in (root / 'benchmarks/runs/20260909-6439-activation-ranks1-50/results').glob('*.json')]
total = sum(r['solve']['wall_seconds'] for r in full)
assert abs(total - 8731.653) < 1e-8

# Refresh only campaigns that were active in the preceding audit, plus the
# newly fully checked current run and its profile. No global/fuzz job query.
accounting = {}
launches = {}
for name, run in prior['runs'].items():
    for key, account in run['accounting'].items():
        active = any(line.split('|')[1] in ('RUNNING', 'PENDING', 'COMPLETING')
                     for line in account['stdout'].splitlines() if '|' in line)
        if active or name in ('6439-activation-ranks1-50', '6372-rank32-release-profile'):
            launches[(name, key)] = account
for (name, key), account in launches.items():
    command = account['command']
    output = subprocess.check_output(command, text=True)
    assert output.strip(), (name, key)
    for line in output.splitlines():
        fields = line.split('|')
        assert fields[1] in ('COMPLETED', 'RUNNING', 'PENDING', 'COMPLETING'), (name, line)
    accounting[name + '/' + key] = dict(command=command, stdout=output, launch=account['launch'])
    print(name, key, 'accounting PASS', flush=True)

checks = {}
expected = 'PASS cases=1022 transitions=5110 conservative_binary_only=9 digest=81aa4d7dfd0f6047\n'
for suffix in ('', '-ubsan'):
    rel = 'benchmarks/probes/scratch/test-binary-retention-contract-6527' + suffix
    result = subprocess.run([str(root / rel)], cwd=root, text=True, capture_output=True, check=True)
    assert result.stdout == expected and not result.stderr
    local[rel] = sha(root / rel)
    checks['fixture' + suffix] = dict(stdout=result.stdout, stderr=result.stderr)
commands = [(n, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + n, '-p', 'test_*.py'])
            for n in ('bench', 'slurm')]
commands += [(n, ['python3', 'tools/docs/' + n + '.py']) for n in (
    'check-doc-size', 'check-progress', 'check-rule-policy-boundary',
    'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(command=command, stdout=result.stdout, stderr=result.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1155-rank32-retention'
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
result = dict(
    observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={r: sha(root / r) for r in sorted(owned)},
    local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'],
    campaigns=campaigns, accounting=accounting, checks=checks,
    rank32=dict(exact_profile_current_non_timing_stats_and_proof=True,
                profile_wall_seconds=profile['solve']['wall_seconds'],
                native_wall_seconds=native['solve']['wall_seconds']),
    full50=dict(solver_seconds=total, fixed_seconds=7239.010, ratio=total / 7239.010),
    andrew_report={str((report / n).relative_to(root)): sha(report / n) for n in ('report.tex', 'report.pdf')},
    cleanup='No new solver job, proof/input generation or manual deletion; no live scratch or fuzz inspection. Scoped final results checked/clean. Prior audit owns other unchanged evidence.',
    failure_classification='No unit, sanitizer, solver, proof or model failure. No excluded measurement or successful snapshot rerun. No production edit or denied transfer/push retry.',
    next='Specific revamp approval pending for three-site binary-retention fast path. Current6424 remains rollback; no implementation or promotion. All50 and memory goal unmet.',
)
with target.open('x') as out:
    out.write(json.dumps(result, indent=2) + '\n')
print(target)
