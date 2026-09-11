#!/usr/bin/env python3
"""Freeze the byte-neutral storage stage and complete GC net assessment."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-single-occurrence-audit-7252.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()

def bind(p, expected=None, own=False):
    digest = sha(p)
    assert expected is None or digest == expected, p
    rel = str(p.relative_to(root))
    hashes[rel] = digest
    if own: owned.add(rel)

def read(p, own=False, dependencies=False):
    bind(p, own=own)
    value = json.loads(p.read_text())
    if dependencies:
        for rel, digest in value.get('hashes', {}).items():
            bind(root / rel, digest)
    return value

prior = read(root / 'benchmarks/reports/2026-09-11-gc-transition-audit-7248.json')
for rel in prior['owned_paths']:
    if rel not in ('PROGRESS.md', 'docs/implementation-notes.md'):
        bind(root / rel, prior['hashes'][rel])
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
for rel in ('PROGRESS.md', 'docs/implementation-notes.md', 'DEVELOPMENT_PLAN.md', 'CLAUDE.md'):
    bind(root / rel, own=rel in ('PROGRESS.md', 'docs/implementation-notes.md'))
for name in ('build-single-occurrence-7249', 'check-single-occurrence-sanitizers-7250',
             'assess-gc-forward-7251', 'audit-single-occurrence-7252'):
    bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
build = root / 'benchmarks/runs/20260911-7249-single-occurrence-build'
manifest = read(build / 'manifest.json', own=True)
source = root / manifest['source']
for rel, digest in manifest['files'].items(): bind(source / rel, digest)
for rel, digest in manifest['binaries'].items(): bind(root / rel, digest)
retained = root / 'build/pin-7162-vivify-revalidation-release/tactix'
bind(retained)
for kind, count in (('release', 116), ('bc', 116), ('trace', 115)):
    assert f'100% tests passed out of {count}' in (build / f'logs/{kind}-ctest.log').read_text()
    if kind != 'trace':
        assert manifest['binaries'][f'build/pin-7249-single-occurrence-{kind}/tactix'] == sha(retained)
for directory in (build, root / 'benchmarks/runs/20260911-7250-single-occurrence-sanitizers'):
    for p in directory.rglob('*'):
        if p.is_file() and p.suffix in ('.json', '.log', '.patch'): bind(p, own=True)
san = read(root / 'benchmarks/runs/20260911-7250-single-occurrence-sanitizers/summary.json', dependencies=True)
assert san['status'] == 'PASS' and len(san['outcomes']['test_connected_occurrence']) == 2
assessment = read(root / 'benchmarks/reports/2026-09-11-gc-forward-assessment-7251.json', own=True, dependencies=True)
assert assessment['runtime_expected'] == 18 and assessment['profile_expected'] == 6
deleted = 0
for wave in range(3):
    p = root / f'benchmarks/runs/20260911-7242-gc-forward-samples/wave{wave}/summary.json'
    value = read(p, own=wave == 2, dependencies=True)
    assert not list(p.parent.glob('expanded-*'))
    if wave == 2: deleted += value['temporary_expansions_deleted_bytes']
    read(root / f'benchmarks/reports/2026-09-11-gc-forward-collection-7243-wave{wave}.json', own=wave == 2, dependencies=True)
bind(root / 'benchmarks/reports/2026-09-11-single-occurrence-checkpoint-7252.md', own=True)
for rel in ('src/kernel/connected_occurrence.hpp', 'tests/test_connected_occurrence.cpp'):
    assert rel in manifest['files']
for rel in ('kissat/src/dense.c', 'kissat/src/watch.c', 'cadical/src/occs.hpp', 'cadical/src/occs.cpp'):
    bind(root / rel)

campaigns, jobs = {}, set()
for name, old in prior['campaigns'].items():
    c = root / 'benchmarks/runs' / name
    m = read(c / 'manifest.json')
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p)
            assert launch['manifest_sha256'] == sha(c / 'manifest.json')
            jobs.add(str(launch['job_id']))
    rows = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = read(p, own=True)
            task = r['task']
            assert task == int(p.stem) and 0 <= task < old['expected']
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            rows[task] = state
    campaigns[name] = dict(expected=old['expected'], published=len(rows),
        checked=sum(r['checked'] for r in rows.values()), records=rows)
assert campaigns['20260910-7167-vivify-revalidation-ranks1-50']['checked'] == 50
assert campaigns['20260911-7238-gc-forward-BC']['checked'] == 66
scheduler = {}
for name, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    scheduler[name] = dict(command=command, exit_code=result.returncode,
                           stdout=result.stdout, stderr=result.stderr)
    if name == 'queue': assert result.returncode == 0, result.stderr
report = root / 'report-to-andrew/20260911-0316-single-occurrence'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py', '--self-test']
             for name in ('assess-gc-forward-7251', 'localize-gc-forward-collection-7243')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-vivify-revalidation-full50-7168', 'validate-factor-arena-full50-7201',
    'check-submit-congruence-fused-runtime-7205', 'validate-congruence-fused-profiles-7206',
    'check-submit-root-binary-runtime-7226', 'validate-root-binary-profiles-7230',
    'check-submit-gc-forward-runtime-7240', 'validate-gc-forward-profiles-7241')]
checks = {}
for command in commands:
    result = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=result.returncode, output=result.stdout + result.stderr)
    assert result.returncode == 0, (command, result.stdout, result.stderr)
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        previous_turn='Progress: committed3a4306fd advanced GC waves and conserving cost diagnoses.',
        decision='Complete7251 GC assessment has local Reduce gain but net31/35 runtime loss, no promotion.7249 implements the tested owning occurrence component without connecting Kernel: Release/BC byte-identical to7162. Full consumer cutover, whole-Eliminate/RSS measurements and correctness gates remain required.',
        newly_deleted_bytes=deleted,
        cleanup='Only the verified third7242 raw expansion was removed this turn; previous7248 owns the first two receipts. Compressed originals/streams, active checkers, failure evidence and all immutable pins remain.',
        limitation='Original convergence target, memory parity, current-source fuzz and consolidation remain open. No new solver job, policy/guard change, fuzz artifact access/transfer or denied push retry.'), out, indent=2)
print(target)
for name, value in campaigns.items(): print(name, value['published'], value['checked'])
