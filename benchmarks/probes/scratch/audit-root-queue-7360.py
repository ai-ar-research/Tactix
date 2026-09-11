#!/usr/bin/env python3
"""One-shot evidence checkpoint; no source promotion or prior artifact rewrite."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-root-queue-audit-7360.json'
assert not target.exists(), 'successful one-shot writer must never be rerun'
hashes, owned = {}, set()
def bind(path, expected=None, own=False):
    digest = sha(path)
    rel = str(path.relative_to(root))
    assert expected is None or digest == expected, rel
    hashes[rel] = digest
    if own: owned.add(rel)

prior_path = root / 'benchmarks/reports/2026-09-11-root-storage-audit-7355.json'
prior = json.loads(prior_path.read_text())
bind(prior_path)
for rel, digest in prior['hashes'].items():
    if rel == 'PROGRESS.md': continue
    assert sha(root / rel) == digest, rel
bind(root / 'PROGRESS.md', own=True)
old_progress = subprocess.check_output(['git', 'show', 'HEAD:PROGRESS.md'], cwd=root, text=True)
progress = (root / 'PROGRESS.md').read_text()
marker = ' Prior accepted frontier/history below remains frozen'
assert old_progress[old_progress.index(marker):] == progress[progress.index(marker):]
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
for name in ('prepare-bve-heap-7356.py', 'check-submit-bve-heap-7356.py',
             'analyze-bve-heap-7357.py', 'prepare-root-queue-7358.py',
             'check-submit-root-queue-7358.py', 'analyze-root-queue-7359.py',
             'audit-root-queue-7360.py'):
    bind(Path(__file__).with_name(name), own=True)
for name in ('2026-09-11-bve-heap-analysis-7357.json',
             '2026-09-11-root-queue-analysis-7359.json',
             '2026-09-11-root-queue-cause-7360.md'):
    bind(root / 'benchmarks/reports' / name, own=True)
    if name.endswith('.json'):
        report = json.loads((root / 'benchmarks/reports' / name).read_text())
        for rel, digest in report['hashes'].items(): bind(root / rel, digest)

campaigns = {}
for name, expected in [('20260911-7356-bve-heap', 2), ('20260911-7358-root-queue', 2),
                       ('20260911-7347-root-storage-BC', 66), ('20260911-7312-clause-full50', 50)]:
    c = root / 'benchmarks/runs' / name
    # Resolve the actual pre-existing full50 directory from the prior audit.
    if not c.exists() and expected == 50:
        matches = [n for n in prior['campaigns'] if n.startswith('20260911-7312')]
        assert len(matches) == 1
        name = matches[0]; c = root / 'benchmarks/runs' / name
    states = {}
    fresh = expected == 2
    if fresh:
        m = json.loads((c / 'manifest.json').read_text())
        for rel, digest in m['dependencies'].items(): bind(root / rel, digest)
        for rel, digest in m['pin_sha256'].items():
            path = c / rel
            with path.open('rb') as stream: elf = stream.read(4) == b'\x7fELF'
            bind(path, digest, own=not elf)
        for path in [c / 'manifest.json', c / 'launch.json', *(c / 'claims').glob('*.json')]:
            bind(path, own=True)
    for folder in ('provisional', 'results'):
        for path in sorted((c / folder).glob('*.json')):
            record = json.loads(path.read_text())
            task = record['task']
            assert task == int(path.stem) and 0 <= task < expected
            state = classify(record, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_UNSAT', 'checked_SAT'), path
            states[task] = state
            bind(path, own=fresh or str(path.relative_to(root)) not in prior['hashes'])
            if fresh and folder == 'results':
                for artifact in record['trace_artifacts'].values(): bind(c / artifact['path'], artifact['sha256'])
    assert len(states) == expected
    if fresh or expected == 50: assert all(s['checked'] and s['clean'] for s in states.values())
    campaigns[name] = dict(expected=expected, published=len(states),
        checked=sum(s['checked'] for s in states.values()), states=states)

debug = root / 'benchmarks/runs/20260911-7342-debug-contracts-debug'
debug_state = 'pending_final_demo; 122 earlier passes, not full-suite acceptance'
for name in ('started.json', 'logs/configure.json', 'logs/build.json'): bind(debug / name)
if (debug / 'manifest.json').exists():
    d = json.loads((debug / 'manifest.json').read_text())
    assert d['status'] == 'PASS' and d['tests'] == 123
    for rel, digest in d['hashes'].items(): bind(root / rel, digest)
    bind(debug / 'manifest.json', own=True); bind(debug / 'logs/ctest.json', own=True)
    debug_state = 'PASS123'

andrew = root / 'report-to-andrew/20260911-1102-root-queue'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(andrew / name)
assert (andrew / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (andrew / 'report.log').read_text()

commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', path, '-p', 'test_*.py']
            for path in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in
             ('check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
              'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name] for name in
             ('validate-clause-full50-7312.py', 'check-submit-bve-heap-7356.py', 'check-submit-root-queue-7358.py')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name, '--self-test'] for name in
             ('analyze-bve-heap-7357.py', 'analyze-root-queue-7359.py')]
checks = {}
for command in commands:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    assert result.returncode == 0, (command, result.stdout, result.stderr)
    checks[' '.join(command)] = dict(exit_code=0, stdout=result.stdout, stderr=result.stderr)
    print('PASS', ' '.join(command), flush=True)
scheduler = {}
for label, command in [
    ('queue', ['squeue', '-h', '-r', '-j', '1681328,1681401,1681403', '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', '1681401,1681403', '-S', '2026-09-11T10:48:00',
                    '-o', 'JobID,State%30,ExitCode,Start', '-P'])]:
    result = subprocess.run(command, text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, result.stderr
    scheduler[label] = dict(command=command, stdout=result.stdout, stderr=result.stderr)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        inherited_authority=str(prior_path.relative_to(root)),
        inherited_immutable_bindings_revalidated=len(prior['hashes']) - 1,
        owned_paths=sorted(owned), campaigns=campaigns, checks=checks, scheduler=scheduler,
        debug_state=debug_state, removed_bytes=0,
        decision='Numeric queue compaction explains selected BVE relevance change; identify missed fixed-variable deactivation before repair. No source promotion or runtime gain.',
        hygiene='All new diagnostic original proofs checked-clean; no new residue deletion. Prior active checker and Debug artifacts preserved, no growing log hashed. Prior audit owns older campaign mapping and source/build bindings.',
        next='Actual-consumer fixed lifecycle/compaction/BVE regression for the29 identities in7359; no parent-stamp offset or restart tuning. Existing pending Debug/BC checks remain asynchronous.',
        limitation='Not goal completion. Latest-source fuzz/consolidation/synchronization remain open under prior denied access/transfer/push boundary; no retry or bypass.'), out, indent=2)
print(target)
