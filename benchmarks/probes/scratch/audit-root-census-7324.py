#!/usr/bin/env python3
"""One-shot checkpoint; preserve the failed exact-RSS gate without promotion."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root / 'benchmarks/reports/2026-09-11-rank25-root-census-audit-7324.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()

def bind(p, expected=None, own=False):
    rel = str(p.relative_to(root))
    digest = sha(p)
    assert expected is None or expected == digest, p
    hashes[rel] = digest
    if own:
        owned.add(rel)

prior_path = root / 'benchmarks/reports/2026-09-11-rank25-root-storage-audit-7321.json'
bind(prior_path)
prior = json.loads(prior_path.read_text())
for rel, digest in prior['hashes'].items():
    if rel != 'PROGRESS.md':
        bind(root / rel, digest)
for rel in ('PROGRESS.md', 'tools/bench/root_census.py',
            'tools/bench/root_census_task.py', 'tools/bench/test_root_census.py',
            'tools/bench/test_root_census_task.py',
            'benchmarks/probes/scratch/prepare-root-census-7322.py',
            'benchmarks/probes/scratch/build-root-census-7322.py',
            'benchmarks/probes/scratch/prepare-root-census-7323.py',
            'benchmarks/probes/scratch/run-root-census-7323.py',
            'benchmarks/probes/scratch/submit-root-census-7323.py',
            'benchmarks/probes/scratch/validate-root-census-7323.py',
            'benchmarks/probes/scratch/audit-root-census-7324.py',
            'benchmarks/reports/2026-09-11-rank25-root-census-7324.md'):
    bind(root / rel, own=True)

build = root / 'benchmarks/runs/20260911-7322-root-census-build'
run = root / 'benchmarks/runs/20260911-7323-rank25-root-census'
bm = json.loads((build / 'manifest.json').read_text())
summary = json.loads((run / 'summary.json').read_text())
for data in (bm, summary):
    for rel, digest in data['hashes'].items():
        bind(root / rel, digest)
for binary in bm['binaries'].values():
    bind(root / binary['path'], binary['sha256'])
assert bm['binaries']['control']['sha256'] == 'fc1a4ca2634c4312b22dc844a1fd7ce5897bae34481f74b362a1220fa1519c54'
assert summary['exact_ordinary_rss_reproduced'] is False
assert len(summary['rows']) == 88
assert len(summary['states']) == 2 and all(s['checked'] for s in summary['states'].values())
for campaign in (build, run):
    for p in sorted(campaign.rglob('*')):
        if not p.is_file() or '__pycache__' in p.parts:
            continue
        # Executables are retained locally and hash-bound, not staged as blobs.
        if p.parent.name == 'pins' and p.suffix != '.py':
            bind(p)
        else:
            bind(p, own=True)

assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
old_progress = subprocess.check_output(['git', 'show', 'HEAD:PROGRESS.md'], cwd=root, text=True)
new_progress = (root / 'PROGRESS.md').read_text()
marker = ' Prior accepted frontier/history below remains frozen'
assert old_progress[old_progress.index(marker):] == new_progress[new_progress.index(marker):]

campaigns, jobs = {}, set()
expected_campaigns = dict(prior['campaigns'])
expected_campaigns[run.name] = dict(expected=2)
for name, old in expected_campaigns.items():
    c = root / 'benchmarks/runs' / name
    states = {}
    for folder in ('provisional', 'results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text())
            bind(p, own=True)
            task = r['task']
            assert task == int(p.stem) and 0 <= task < old['expected']
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

report = root / 'report-to-andrew/20260911-0846-rank25-census'
for name in ('report.tex', 'report.pdf', 'report.log'):
    bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, f'tools/docs/{name}.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-root-census-7323', 'validate-clause-full50-7312')]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, text=True, capture_output=True)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout + p.stderr)
    print('PASS', ' '.join(command), flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, scheduler=scheduler, checks=checks,
        decision='Preserve isolated census and negative exact-RSS gate; external ordinary-binary confirmation precedes exact peak attribution. No production promotion.',
        exact_ordinary_rss_reproduced=False, newly_deleted_bytes=0,
        limitation='Observer state is not instantaneous peak RSS or object liveness. Pending proofs asynchronous; current-source fuzz, goal and consolidation open. No denied access/transfer or push retry.'), out, indent=2)
print(target)
