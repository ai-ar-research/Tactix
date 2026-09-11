#!/usr/bin/env python3
"""Freeze completed observer evidence; never freeze actively growing traces."""
import datetime as dt
import gzip
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-external-memory-audit-7296.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    rel = str(p.relative_to(root))
    if rel not in hashes:
        hashes[rel] = sha(p)
    assert expected is None or hashes[rel] == expected, p
    if own:
        owned.add(rel)
def read(p, own=False):
    bind(p, own=own)
    return json.loads(p.read_text())
erratum = read(root / 'benchmarks/reports/2026-09-11-memory-log-snapshot-erratum-7293.json')
excluded = set(erratum['excluded_live_paths'])
assert len(excluded) == 2
prior = read(root / 'benchmarks/reports/2026-09-11-memory-only-audit-7290.json')
for rel in prior['owned_paths']:
    if rel != 'PROGRESS.md' and rel not in excluded:
        bind(root / rel, prior['hashes'][rel])
packed = read(root / 'benchmarks/reports/2026-09-11-memory-only-landing-7292.json')
for record in packed['records']:
    a, b = root / record['canonical'], root / record['packed']
    bind(a, record['canonical_sha256'])
    bind(b, record['packed_sha256'])
    assert gzip.decompress(b.read_bytes()) == a.read_bytes()
for rel in ('PROGRESS.md', 'CLAUDE.md', 'DEVELOPMENT_PLAN.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
for rel in (
    'tools/bench/syscall_memory_task.py', 'tools/bench/test_syscall_memory_task.py',
    'benchmarks/probes/scratch/compare-memory-only-7294.py',
    'benchmarks/probes/scratch/prepare-memory-syscalls-7295.py',
    'benchmarks/probes/scratch/submit-memory-syscalls-7295.py',
    'benchmarks/probes/scratch/validate-memory-syscalls-7295.py',
    'benchmarks/probes/scratch/audit-external-memory-7296.py',
    'benchmarks/reports/2026-09-11-memory-observer-reassessment-7296.md'):
    bind(root / rel, own=True)
for name in ('summary.json', 'comparison-7294.json'):
    p = root / 'benchmarks/runs/20260911-7286-memory-only-rank31' / name
    data = read(p, own=True)
    for rel, digest in data['hashes'].items():
        bind(root / rel, digest)
comparison = data
assert comparison['exact_structural_rows'] == 10538
assert comparison['observer_peaks_kib'] == {'candidate': 509084, 'previous': 510784}
assert not comparison['reproduction']['adverse_direction_reproduced']
assert comparison['reproduction']['delta_error_kib'] == -69464

expected = {name: data['expected'] for name, data in prior['campaigns'].items()}
new_name = '20260911-7295-memory-syscalls-rank31'
expected[new_name] = 2
campaigns, jobs = {}, set()
for name, count in expected.items():
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json', own=name == new_name)
    for rel, digest in m['pin_sha256'].items():
        bind(c / rel, digest, own=name == new_name and (c / rel).suffix in ('.py', '.tsv', '.sbatch'))
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p, own=name == new_name)
            jobs.add(str(launch['job_id']))
            assert launch['manifest_sha256'] == hashes[str((c / 'manifest.json').relative_to(root))]
    states = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            record = read(p, own=True)
            task = record['task']
            assert task == int(p.stem) and 0 <= task < count
            state = classify(record, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            states[task] = state
            # Only a completed solver record may bind its completed trace.
            if name in ('20260911-7286-memory-only-rank31', new_name):
                key = 'memory_syscalls' if name == new_name else 'memory_trace'
                trace = record['solve'][key]
                q = c / trace['path']
                assert q.parent == c / 'logs' and q.stat().st_size == trace['bytes']
                bind(q, trace['sha256'], own=True)
    campaigns[name] = dict(expected=count, published=len(states),
        checked=sum(s['checked'] for s in states.values()), records=states)
    if name == new_name:
        for rel, digest in m['evidence_sha256'].items():
            bind(root / rel, digest)
        for p in (c / 'logs').glob('preflight.*'):
            bind(p, own=True)
assert campaigns['20260911-7286-memory-only-rank31']['published'] == 2
scheduler = {}
for label, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    p = subprocess.run(command, capture_output=True, text=True, timeout=30)
    scheduler[label] = dict(command=command, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)
    assert p.returncode == 0, (label, p.stderr)
report = root / 'report-to-andrew/20260911-0626-external-memory'
for name in ('report.tex', 'report.pdf', 'report.log'):
    bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py', '--self-test'] for name in (
    'compare-memory-only-7294', 'validate-memory-syscalls-7295')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-occurrence-full50-7262', 'check-submit-dense-realloc-runtime-7273',
    'validate-realloc-rank31-memory-7277', 'validate-realloc-full50-7283',
    'validate-memory-only-rank31-7286', 'compare-memory-only-7294',
    'validate-memory-syscalls-7295')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        previous_turn='Progress: completed full50 tradeoff, memory-only build/control validation, diagnostic launch and live-log snapshot correction. Current turn establishes non-reproduction and changes the next measurement to exact Release external syscalls.',
        decision='Two in-process observers reverse the Release Rank31 RSS penalty. Stop assigning Release peaks from those traces.7295 externally traces exact7267/7253 binaries with no preload/source instrumentation; require whole-work/certificates and RSS reproduction before allocator attribution.',
        newly_deleted_bytes=0, cleanup='Nothing deleted.7293 live-log hash exclusions applied explicitly; completed7286 records now bind final trace hashes. No growing7295 logs owned without a solver record. Canonical patch/gzip evidence remains unchanged.',
        test_consideration='Six new external-wrapper tests and five peak-reproduction rejection controls, plus existing structural and fingerprint guards. Actual fixed allocation fixture passes pinned-strace preflight. No solver-source change or gate waiver.',
        limitation='Runtime target, memory parity, independent checks/zero residue, current-source Claude-owned fuzz and consolidation remain open. No denied fuzz access/transfer or push retry. No syscall-profile or RSS-success claim before completed solver evidence.'), out, indent=2)
print(target)
