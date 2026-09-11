#!/usr/bin/env python3
"""Freeze the root-storage checkpoint, including failed gates; no promotion."""
import datetime as dt
import difflib
import gzip
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-root-fold-audit-7338.json'
packaging = root / 'benchmarks/runs/20260911-7338-root-fold-checkpoint'
assert not target.exists() and not packaging.exists(), 'one-shot evidence writer'
hashes, owned = {}, set()
def bind(path, expected=None, own=False):
    rel, digest = str(path.relative_to(root)), sha(path)
    assert expected is None or digest == expected, (rel, digest, expected)
    hashes[rel] = digest
    if own: owned.add(rel)

prior_path = root / 'benchmarks/reports/2026-09-11-external-root-state-audit-7328.json'
prior = json.loads(prior_path.read_text())
bind(prior_path)
changed_docs = {'PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md'}
for rel, digest in prior['hashes'].items():
    if rel not in changed_docs: bind(root / rel, digest)
for rel in changed_docs: bind(root / rel, own=True)
bind(root / 'benchmarks/reports/2026-09-11-root-fold-checkpoint-7338.md', own=True)
helpers = ('prepare-root-fold-7330.py', 'check-root-fold-7331.py',
           'build-root-fold-7332.py', 'prepare-root-watermark-7333.py',
           'reproduce-root-watermark-7334.py', 'build-root-watermark-7335.py',
           'check-root-watermark-proofs-7336.py', 'check-root-parent-debug-7337.py',
           'audit-root-fold-7338.py')
for name in helpers: bind(Path(__file__).with_name(name), own=True)

build_path = root / 'benchmarks/runs/20260911-7335-root-watermark-build/manifest.json'
build = json.loads(build_path.read_text())
assert build['status'] == 'PASS'
source = root / build['source']
for rel, digest in build['files'].items(): bind(source / rel, digest)
for values in build['binaries'].values():
    for rel, digest in values.items(): bind(root / rel, digest)
assert list(build['binaries']['release'].values()) == list(build['binaries']['bc'].values())
proof_path = root / 'benchmarks/runs/20260911-7336-root-watermark-proofs/manifest.json'
proofs = json.loads(proof_path.read_text())
assert proofs['status'] == 'PASS' and proofs['checked_UNSAT'] == proofs['checked_SAT'] == 64
assert proofs['negative_controls'] == 2 and proofs['exact_fold_disabled_certificates']
regression = json.loads((root / 'benchmarks/runs/20260911-7334-root-watermark-regression/manifest.json').read_text())
assert regression['status'] == 'REPRODUCED'
debug = json.loads((root / 'benchmarks/runs/20260911-7337-root-parent-debug/manifest.json').read_text())
assert debug['status'] == 'REPRODUCED_ON_UNCHANGED_PARENT'
for data in (build, proofs, regression, debug):
    for rel, digest in data['hashes'].items(): bind(root / rel, digest)
for name in ('20260911-7331-root-fold-preflight', '20260911-7332-root-fold-build',
             '20260911-7333-root-watermark-prepare', '20260911-7334-root-watermark-regression',
             '20260911-7335-root-watermark-build', '20260911-7336-root-watermark-proofs',
             '20260911-7337-root-parent-debug'):
    for path in sorted((root / 'benchmarks/runs' / name).rglob('*')):
        if not path.is_file() or '__pycache__' in path.parts: continue
        with path.open('rb') as stream: executable = stream.read(4) == b'\x7fELF'
        bind(path, own=not executable and path.name != 'incremental.patch')
# Source7330 is frozen by the prepare7333 parent-file snapshot even though its
# failed preflight intentionally never wrote a success manifest.
prepared = json.loads((root / 'benchmarks/runs/20260911-7333-root-watermark-prepare/manifest.json').read_text())
for rel, digest in prepared['parent_files'].items(): bind(root / prepared['parent'] / rel, digest)
old_debug_log = root / 'benchmarks/runs/20260911-7331-root-fold-preflight/logs/Debug-ctest.json'
old_debug = json.loads(old_debug_log.read_text())
assert old_debug['returncode'] != 0 and '6 tests failed out of 121' in old_debug['stdout']
for name in ('forward_unit', 'substitute_collapse', 'substitute_retry', 'subsume_material'):
    failure = next(r for r in debug['records'] if r['label'] == name)
    assertion = failure['stderr'].split('Assertion ', 1)[1]
    assert assertion.strip() in old_debug['stdout'], name

packaging.mkdir()
parent = root / '.claude/worktrees/runtime-clause-realloc-7301'
for name, base in (('incremental.patch', parent), ('cumulative.patch', root)):
    patch = ''
    for rel in sorted(build['files']):
        old, new = base / rel, source / rel
        if old.exists() and sha(old) == sha(new): continue
        patch += ''.join(difflib.unified_diff(old.read_text().splitlines(True) if old.exists() else [],
                         new.read_text().splitlines(True), fromfile='a/' + rel if old.exists() else '/dev/null',
                         tofile='b/' + rel, n=0))
    with (packaging / name).open('x') as out: out.write(patch)
    bind(packaging / name, own=True)
raw = root / 'benchmarks/runs/20260911-7331-root-fold-preflight/incremental.patch'
packed = packaging / 'original-7331-incremental.patch.gz'
with packed.open('xb') as out: out.write(gzip.compress(raw.read_bytes(), mtime=0))
assert gzip.decompress(packed.read_bytes()) == raw.read_bytes()
bind(packed, own=True)
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
old_progress = subprocess.check_output(['git', 'show', 'HEAD:PROGRESS.md'], cwd=root, text=True)
new_progress = (root / 'PROGRESS.md').read_text()
marker = ' Prior accepted frontier/history below remains frozen'
assert old_progress[old_progress.index(marker):] == new_progress[new_progress.index(marker):]

campaigns, jobs = {}, set()
for name, previous in prior['campaigns'].items():
    campaign = root / 'benchmarks/runs' / name
    states = {}
    for folder in ('provisional', 'results'):
        for path in sorted((campaign / folder).glob('*.json')):
            record = json.loads(path.read_text())
            task = record['task']
            assert task == int(path.stem) and 0 <= task < previous['expected']
            state = classify(record, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (path, state)
            states[task] = state
            bind(path, own=True)
    campaigns[name] = dict(expected=previous['expected'], published=len(states),
                           checked=sum(s['checked'] for s in states.values()), records=states)
    for path in campaign.iterdir():
        if path.name == 'launch.json' or path.name.endswith('-submission.json'):
            jobs.add(str(json.loads(path.read_text())['job_id']))
scheduler = {}
for name, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    result = subprocess.run(command, text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, result.stderr
    scheduler[name] = dict(command=command, stdout=result.stdout, stderr=result.stderr)
report = root / 'report-to-andrew/20260911-0954-root-fold-checkpoint'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', path, '-p', 'test_*.py']
            for path in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/validate-clause-full50-7312.py']]
checks = {}
for command in commands:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    assert result.returncode == 0, (command, result.stdout, result.stderr)
    checks[' '.join(command)] = dict(exit_code=result.returncode, output=result.stdout + result.stderr)
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
                   owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
                   source=build['source'], source_binaries=build['binaries'],
                   checked_generated_bytes_removed=proofs['removed_bytes'],
                   failed_debug_tests=5, intentionally_stopped_obsolete_debug_test=1,
                   patch_application='git apply --unidiff-zero',
                   decision='Keep isolated root-storage successor and actual regression fixes; inherited Debug failures remain open before broad gates. No runtime/memory gain or source promotion.',
                   limitation='No new real benchmark; pending full7312 proof remains asynchronous. Current-source fuzz, full50/consolidation and synchronization remain open. No denied access/transfer/push retry.'), out, indent=2)
print(target)
