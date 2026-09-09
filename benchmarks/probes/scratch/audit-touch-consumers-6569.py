#!/usr/bin/env python3
"""Audit fixed baseline tests and design authority; no new solver/job/fuzz access."""
import datetime
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-touch-consumer-audit-6569.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-current-material-pair-audit-6565.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-09-material-tracker-design-and-touch-contract.md'}
owned.update('benchmarks/probes/scratch/' + name for name in (
    'test-touch-consumers-6567.cpp', 'check-touch-consumers-6568.py',
    'audit-touch-consumers-6569.py', 'land-touch-consumers-6570.py'))
local = {}
def bind(path, expected=None):
    digest = sha(path)
    if expected is not None:
        assert digest == expected, path
    local[str(path.relative_to(root))] = digest
    return digest
def read(path):
    bind(path)
    return json.loads(path.read_text())

b = read(root / 'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json')
for rel, digest in b['files'].items():
    bind(root / b['source'] / rel, digest)
for rel, digest in b['binaries'].items():
    bind(root / rel, digest)
n = read(root / 'benchmarks/runs/20260909-6550-native-material-observer-build/manifest.json')
for rel, digest in n['files'].items():
    bind(root / n['source'] / rel, digest)
cadical_files = ['src/internal.hpp', 'src/clause.cpp', 'src/subsume.cpp', 'src/flags.hpp']
cadical_commit = subprocess.check_output(['git', '-C', str(root / 'cadical'), 'rev-parse', 'HEAD'], text=True).strip()
assert cadical_commit == '7b99c07f0bcab5824a5a3ce62c7066554017f641'
assert not subprocess.check_output(['git', '-C', str(root / 'cadical'), 'status', '--short', '--', *cadical_files])
for rel in cadical_files:
    bind(root / 'cadical' / rel)

c = root / 'benchmarks/runs/20260909-6568-touch-consumer-contract'
s = read(c / 'summary.json')
assert s['cases_per_build'] == 1300 and s['exhaustive_snapshots_per_build'] == 6480
for rel, digest in s['hashes'].items():
    bind(root / rel, digest)
for name, check in s['checks'].items():
    assert name in ('optimized', 'ubsan') and check['exit_code'] == 0 and not check['stderr']
    bind(c / name, check['binary_sha256'])
    assert read(c / (name + '-run.json')) == check
assert set(s['checks']) == {'optimized', 'ubsan'}
assert {p.name for p in c.iterdir()} == {'optimized', 'ubsan', 'optimized-build.log', 'ubsan-build.log',
    'optimized-run.json', 'ubsan-run.json', 'summary.json'}
for p in c.iterdir():
    bind(p)
    if p.suffix in ('.json', '.log'):
        owned.add(str(p.relative_to(root)))
print('1300 cases / 6480 exhaustive snapshots per build; both controls verified', flush=True)

campaigns = {}
for name, expected in (
    ('20260908-6108-rank41-50-matched', 20),
    ('20260909-6439-activation-ranks1-50', 50),
    ('20260909-6535-rank32-congruence-time', 1),
    ('20260909-6539-rank32-early-congruence-time', 1),
):
    paths = sorted((root / 'benchmarks/runs' / name / 'results').glob('*.json'))
    rows = [read(p) for p in paths]
    assert len(rows) == len({r['task'] for r in rows}) == expected
    for r in rows:
        assert classify(r, True)['status'] in ('checked_SAT', 'checked_UNSAT')
        assert r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
    campaigns[name] = dict(checked_clean=expected)
    print(name, expected, 'checked/clean', flush=True)
fixed = read(root / 'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json')
assert fixed['solver_seconds'] == 7239.010
# Already terminal accounting is authoritative; no new job was submitted and
# no absent/live process is inferred from a lock or queue disappearance.
for name, account in prior['accounting'].items():
    assert account['stdout'].strip(), name
    for line in account['stdout'].splitlines():
        fields = line.split('|')
        assert fields[1] == 'COMPLETED' and fields[2] == '0:0', (name, line)

checks = {}
commands = [(name, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + name, '-p', 'test_*.py'])
            for name in ('bench', 'slurm')]
commands += [(name, ['python3', 'tools/docs/' + name + '.py']) for name in (
    'check-doc-size', 'check-progress', 'check-rule-policy-boundary',
    'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, command in commands:
    run = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=run.stdout, stderr=run.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1340-material-tracker-design'
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel: sha(root / rel) for rel in sorted(owned)}, local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'], campaigns=campaigns,
    accounting=prior['accounting'], checks=checks, full50=prior['full50'],
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    reference_cadical_commit=cadical_commit, fixture=dict(cases_per_build=1300, snapshots_per_build=6480, checks=s['checks']),
    cleanup='No new solver, job, CNF, certificate or scratch artifact. Fixed binaries/logs retained as contract evidence. Prior6535/6539 both independently checked and cleanup complete. No live scratch or fuzz access.',
    failure_classification='Both fixed builds and their first executions pass. No solver/proof/model failure, result exclusion, successful writer rerun, source/campaign mutation or denied transfer/push retry.',
    next='Obtain explicit approval before implementing the mapped separate material flag/counter and canonical scheduling consumer. No new native replay of the same Rank50 boundary. Goal unmet; fuzz and promotion obligations remain open.')
with target.open('x') as out:
    json.dump(result, out, indent=2)
    out.write('\n')
print(target)
