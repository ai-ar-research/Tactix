#!/usr/bin/env python3
"""Freeze a read-only checkpoint; never submit, cancel, inspect scratch or fuzz."""
from collections import Counter
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha

target = root / 'benchmarks/reports/2026-09-10-largest-gap-audit-7005.json'
assert not target.exists(), 'successful audit is frozen'
c = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles'
m = json.loads((c / 'manifest.json').read_text())
hashes = {}
def bind(path, expected=None):
    actual = sha(path)
    assert expected is None or actual == expected, path
    hashes[str(path.relative_to(root))] = actual

for rel, digest in m['pin_sha256'].items():
    bind(c / rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in m[group].items():
        bind(root / rel, digest)
for item in m['unprofiled_records'].values():
    bind(root / item['path'], item['sha256'])
for name in ('manifest.json', 'launch.json'):
    bind(c / name)
for rel in ('20260909-6730-reduce-watermark-build',
            '20260910-6950-xor-presence-build',
            '20260910-6969-ite-pair-hoist-build'):
    p = root / 'benchmarks/runs' / rel / 'manifest.json'
    build = json.loads(p.read_text()); bind(p)
    for name, digest in build['files'].items():
        bind(root / build['source'] / name, digest)
    for name, digest in build['binaries'].items():
        bind(root / name, digest)

owned = ['PROGRESS.md', 'benchmarks/reports/2026-09-10-largest-gap-priority.md',
         'benchmarks/probes/scratch/prepare-largest-gap-profiles-7002.py',
         'benchmarks/probes/scratch/submit-largest-gap-profiles-7003.py',
         'benchmarks/probes/scratch/validate-largest-gap-profiles-7004.py',
         str(Path(__file__).relative_to(root))]
for rel in owned:
    bind(root / rel)
report = root / 'report-to-andrew/20260910-0740-largest-gap'
for name in ('report.tex', 'report.pdf', 'report.log'):
    bind(report / name)
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').stat().st_size > 1000

checks = {}
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory,
             '-p', 'test_*.py'] for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in
             ('check-doc-size', 'check-progress', 'check-section-refs',
              'check-rule-policy-boundary', 'check-default-strategy-config',
              'check-proof-rule-sync')]
for command in commands:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    checks[' '.join(command)] = dict(exit_code=result.returncode,
                                     output=result.stdout + result.stderr)
    assert result.returncode == 0, command

campaigns = {}
for rel, count in (('20260909-6743-reduce-watermark-ranks1-50', 50),
                   ('20260910-6980-xor-presence-ranks1-50', 50),
                   ('20260910-6991-ite-pair-BC-gate', 66),
                   ('20260910-6994-ite-pair-runtime', 18),
                   ('20260910-7002-largest-gap-release-profiles', 4)):
    directory = root / 'benchmarks/runs' / rel
    states = Counter(); pending = []
    for task in range(count):
        p = directory / f'results/{task:03d}.json'
        if not p.exists():
            p = directory / f'provisional/{task:03d}.json'
        record = json.loads(p.read_text()) if p.exists() else None
        state = classify(record, p.parent.name == 'results')
        assert state['status'] in ('checked_UNSAT', 'checked_SAT',
                                    'pending_validation', 'pending_solver'), (p, state)
        states[state['status']] += 1
        if record:
            bind(p)
        if not state['checked']:
            pending.append(task)
    campaigns[rel] = dict(states=dict(states), pending_tasks=pending)
observations = {}
for name, command in (
    ('queue', ['squeue', '-j', '1679638,1679487', '-o', '%.20i %.12T %.10M %.10l %.18R']),
    ('old_allocation', ['sacct', '-X', '-j', '1668242', '--starttime=2026-09-07T00:00:00',
                        '--format=JobID,State,End', '-P'])):
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    observations[name] = result.stdout
assert '1668242|COMPLETED|2026-09-10T07:26:54' in observations['old_allocation']
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
           owned_paths=owned, checks=checks, campaigns=campaigns, scheduler=observations,
           decision='Prioritize31/35 dominant cost/excess work; defer6969full50. No solver edit, reference refresh, promotion or speedup claim.',
           cleanup='No deletion; no checker scratch or fuzz artifact inspection.',
           session='Explicit user resume overrides old cutoff; replacement stopping time requested and pending.',
           report='Local and unsent; no denied push/transfer retry.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps(campaigns, indent=2))
