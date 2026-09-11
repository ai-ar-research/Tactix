#!/usr/bin/env python3
"""Freeze the external-root-state diagnostic; no exact-RSS acceptance."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from proc_memory_analysis import smaps
target = root/'benchmarks/reports/2026-09-11-external-root-state-audit-7328.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    rel, digest = str(p.relative_to(root)), sha(p)
    assert expected is None or digest == expected, p
    hashes[rel] = digest
    if own: owned.add(rel)
prior_path = root/'benchmarks/reports/2026-09-11-rank25-root-census-audit-7324.json'
bind(prior_path)
prior = json.loads(prior_path.read_text())
for rel, digest in prior['hashes'].items():
    if rel != 'PROGRESS.md': bind(root/rel, digest)
for name in ('root_gdb.py', 'root_gdb_task.py', 'root_memory.py',
             'test_root_gdb_task.py', 'test_root_memory.py'):
    bind(root/'tools/bench'/name, own=True)
for name in ('root-layout-7326.cpp', 'build-root-gdb-7326.py', 'finalize-root-gdb-7326.py',
             'prepare-root-gdb-7327.py', 'run-root-gdb-7327.py', 'submit-root-gdb-7327.py',
             'validate-root-gdb-7327.py', 'audit-external-root-state-7328.py'):
    bind(Path(__file__).with_name(name), own=True)
bind(root/'PROGRESS.md', own=True)
bind(root/'benchmarks/reports/2026-09-11-rank25-external-root-state-7328.md', own=True)
bind(root/'cadical/src/compact.cpp')
bind(root/'kissat/src/compact.c')
for name in ('kernel/kernel.cpp', 'api/api.cpp'):
    bind(root/'.claude/worktrees/runtime-clause-realloc-7301/src'/name)
bind(root/'.claude/worktrees/runtime-clause-realloc-7301/tests/test_compact_stamp.cpp')
for name in ('20260911-7326-root-gdb-fixture', '20260911-7327-rank25-root-gdb'):
    c = root/'benchmarks/runs'/name
    data = json.loads((c/'summary.json').read_text())
    for rel, digest in data['hashes'].items(): bind(root/rel, digest)
    for p in sorted(c.rglob('*')):
        if not p.is_file() or '__pycache__' in p.parts: continue
        with p.open('rb') as stream: executable = stream.read(4) == b'\x7fELF'
        bind(p, own=not executable)
assert data['exact_ordinary_rss_reproduced'] is False
assert len(data['states']) == 2 and all(s['checked'] for s in data['states'].values())
snapshot = data['snapshot']
regions = smaps(snapshot['smaps'])
containers = {}
for name, address in (('old_reconstruction', snapshot['old_reconstruction']['address']),
                      ('new_reconstruction', snapshot['new_reconstruction_address'])):
    matches = [r for r in regions if r['start'] <= address < r['end']]
    assert len(matches) == 1
    containers[name] = matches[0]
assert snapshot['state']['populations']['fixed_active'] == 3015698
assert sum(r['kib']['Rss'] for r in regions) == 2485180
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
old = subprocess.check_output(['git', 'show', 'HEAD:PROGRESS.md'], cwd=root, text=True)
new = (root/'PROGRESS.md').read_text()
marker = ' Prior accepted frontier/history below remains frozen'
assert old[old.index(marker):] == new[new.index(marker):]
campaigns, jobs = {}, set()
expected = dict(prior['campaigns'])
expected['20260911-7327-rank25-root-gdb'] = dict(expected=2)
for name, old in expected.items():
    c = root/'benchmarks/runs'/name
    states = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c/folder).glob('*.json')):
            r = json.loads(p.read_text()); bind(p, own=True)
            task = r['task']; assert task == int(p.stem) and 0 <= task < old['expected']
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            states[task] = state
    campaigns[name] = dict(expected=old['expected'], published=len(states),
        checked=sum(s['checked'] for s in states.values()), records=states)
    for p in c.iterdir():
        if p.name == 'launch.json' or p.name.endswith('-submission.json'):
            jobs.add(str(json.loads(p.read_text())['job_id']))
scheduler = {}
for name, command in (
    ('queue', ['squeue', '-h', '-r', '-j', ','.join(sorted(jobs)), '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', ','.join(sorted(jobs)),
                   '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'])):
    p = subprocess.run(command, text=True, capture_output=True, timeout=30)
    assert p.returncode == 0, (command, p.stderr)
    scheduler[name] = dict(command=command, stdout=p.stdout, stderr=p.stderr)
report = root/'report-to-andrew/20260911-0907-external-root-state'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report/name)
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-root-gdb-7327', 'validate-clause-full50-7312')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, text=True, capture_output=True)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout+p.stderr)
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        snapshot_regions=regions, reconstruction_containers=containers,
        exact_ordinary_rss_reproduced=False, newly_deleted_bytes=0,
        decision='Proceed from source-backed root retention to isolated hot/cold-state mini-design; no more instrumentation solely chasing exact RSS. No peak-decomposition acceptance or source promotion.',
        limitation='GNU-time/proc counter discrepancy and observer perturbation retained; merged VMA RSS not split. Goal, current-source fuzz and consolidation open; checks asynchronous. No denied access/transfer or push retry.'), out, indent=2)
print(target)
