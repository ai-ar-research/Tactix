#!/usr/bin/env python3
"""Approved state-only checkpoint: tests, exact paired work, asynchronous checks."""
import datetime
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-09-material-state-audit-6577.json'
assert not target.exists()
prior_path = root / 'benchmarks/reports/2026-09-09-touch-consumer-audit-6569.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md':
            assert sha(root / rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-09-approved-material-state.md'}
owned.update('benchmarks/probes/scratch/' + name for name in (
    'prepare-material-state-6571.py', 'build-material-state-6572.py',
    'control-material-state-6573.py', 'prepare-material-state-BC-6574.py',
    'submit-material-state-BC-6575.py', 'validate-material-state-BC-6576.py',
    'audit-material-state-6577.py', 'land-material-state-6578.py'))
local = {}
def bind(path, expected=None, own=False):
    digest = sha(path)
    if expected is not None:
        assert digest == expected, path
    rel = str(path.relative_to(root))
    local[rel] = digest
    if own:
        owned.add(rel)
    return digest
def read(path, own=False):
    bind(path, own=own)
    return json.loads(path.read_text())

build_path = root / 'benchmarks/runs/20260909-6571-material-state-build'
b = read(build_path / 'manifest.json', True)
approval = read(build_path / 'parent.json', True)
assert approval['approval'].startswith('User replied Yes')
parent = read(root / approval['path'])
assert sha(root / approval['path']) == approval['sha256']
bind(root / approval['design']['path'], approval['design']['sha256'])
for image in (b, parent):
    for rel, digest in image['files'].items():
        bind(root / image['source'] / rel, digest)
    for rel, digest in image['binaries'].items():
        bind(root / rel, digest)
for rel, digest in b['patches'].items():
    bind(build_path / rel, digest, True)
for rel, digest in b['logs'].items():
    bind(build_path / rel, digest, True)
for kind, count in (('release', 109), ('bc', 109), ('trace', 108)):
    assert f'100% tests passed out of {count}' in (build_path / 'logs' / (kind + '-ctest.log')).read_text()
assert b['binaries']['build/pin-6571-material-state-release/tactix'] == b['binaries']['build/pin-6571-material-state-bc/tactix']

control = root / 'benchmarks/runs/20260909-6573-material-shared-touch-control'
s = read(control / 'summary.json', True)
assert s['cases_per_build'] == 1300 and s['exhaustive_snapshots_per_build'] == 6480
for rel, digest in s['hashes'].items():
    bind(root / rel, digest)
for name, check in s['checks'].items():
    assert check['exit_code'] == 0 and not check['stderr']
    bind(control / name, check['binary_sha256'])
for path in control.iterdir():
    bind(path, own=path.suffix in ('.json', '.log'))

c = root / 'benchmarks/runs/20260909-6574-material-state-BC-gate'
m = read(c / 'manifest.json', True)
launch = read(c / 'launch.json', True)
assert launch['manifest_sha256'] == sha(c / 'manifest.json')
neutrality = read(c / 'neutrality-6576.json', True)
assert neutrality['all_work_and_certificates_equal'] and len(neutrality['pairs']) == 33
for rel, digest in neutrality['hashes'].items():
    bind(root / rel, digest)
for rel, digest in m['pin_sha256'].items():
    bind(c / rel, digest, rel.endswith(('.py', '.tsv', '.sbatch', '.patch')))
records = []
for path in sorted((c / 'results').glob('*.json')):
    r = read(path, True)
    status = classify(r, True)
    assert status['checked'] and status['clean'], (path, status)
    records.append(r)
for path in sorted((c / 'provisional').glob('*.json')):
    read(path, True)
command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
           '-o', 'JobID,State%40,ExitCode,Submit', '-P']
output = subprocess.check_output(command, text=True)
lines = output.splitlines()
assert len(lines) == 66
for line in lines:
    f = line.split('|')
    assert f[1] in ('COMPLETED', 'RUNNING', 'COMPLETING') and f[2] == '0:0', line
accounting = dict(prior['accounting'])
accounting[c.name] = dict(command=command, stdout=output, launch=launch)
print('B/C33 exact work/certificate pairs;', len(records), '/66 independently checked/clean', flush=True)

module_path = root / 'benchmarks/probes/scratch/validate-material-state-BC-6576.py'
spec = importlib.util.spec_from_file_location('neutral6576', module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
module.fixtures()
checks = {}
commands = [(name, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/' + name, '-p', 'test_*.py'])
            for name in ('bench', 'slurm')]
commands += [(name, ['python3', 'tools/docs/' + name + '.py']) for name in (
    'check-doc-size', 'check-progress', 'check-rule-policy-boundary',
    'check-proof-rule-sync', 'check-section-refs', 'check-citations')]
for name, cmd in commands:
    result = subprocess.run(cmd, cwd=root, text=True, capture_output=True, check=True)
    checks[name] = dict(command=cmd, stdout=result.stdout, stderr=result.stderr)
    print(name, 'PASS', flush=True)
report = root / 'report-to-andrew/20260909-1535-approved-material-state'
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel: sha(root / rel) for rel in sorted(owned)}, local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'], accounting=accounting,
    checks=checks, full50=prior['full50'], approval=approval['approval'],
    stage='Separate material bookkeeping implemented, scheduling consumers unchanged.',
    gate=dict(paired_cases=33, exact_reported_work_and_certificates=True,
              checked_clean=len(records), independent_validation_pending=len(records) != 66),
    andrew_report={str((report / name).relative_to(root)): sha(report / name) for name in ('report.tex', 'report.pdf')},
    cleanup='Completed B/C records require checked models/proofs and zero runner residue. Pending checker scratch untouched. Fixed state/control tests create no CNF/certificate. No manual deletion, source fuzz access or reference refresh.',
    failure_classification='All build suites and fixed controls pass. The first read-only neutrality status invocation expected binary_sha256 in the intentionally reduced provisional schema; corrected before any summary. Reused shared-touch fixture text says no material-tracker implementation: this describes its old coverage, not the newly linked bookkeeping source. Neither event is a solver/proof/model failure or exclusion.',
    next='Clone frozen6571 for approved canonical C2 flag consumer and Eliminate material watermark cutover, preserving public instances, interruptions and SP-SLOT-5. Keep independent checking asynchronous. Structural tracker approval is granted; FlatLists/fuzz/push authority is not. Full50/memory goal unmet.')
with target.open('x') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
print(target)
