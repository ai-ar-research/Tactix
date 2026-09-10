#!/usr/bin/env python3
"""Seal the isolated repair checkpoint without treating pending checks as passes."""
from collections import Counter
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-10-collect-retry-audit-7037.json'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    value = sha(p)
    assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
owned = ['PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
         'benchmarks/reports/2026-09-10-collect-retry-repair.md']
owned += ['benchmarks/probes/scratch/' + name + '.py' for name in (
    'prepare-collect-retry-7030', 'build-collect-retry-7031',
    'localize-collect-retry-7032', 'check-collect-retry-proofs-7033',
    'prepare-collect-retry-BC-7034', 'submit-collect-retry-BC-7035',
    'validate-collect-retry-BC-7036', 'audit-collect-retry-7037')]
build = root / 'benchmarks/runs/20260910-7030-collect-retry-build'
m = json.loads((build / 'manifest.json').read_text())
for rel, digest in m['files'].items(): bind(root / m['source'] / rel, digest)
for rel, digest in m['binaries'].items(): bind(root / rel, digest)
for rel, digest in {**m['patches'], **m['logs']}.items(): bind(build / rel, digest)
owned += [str(p.relative_to(root)) for p in sorted(build.rglob('*'))
          if p.is_file() and p.suffix in ('.json', '.patch', '.log')]
for rel in ('20260910-7024-rank35-retry-producer/replay-summary-7026.json',
            '20260910-7024-rank35-retry-producer/producer-localization-7032.json',
            '20260910-7033-collect-retry-proofs/summary.json'):
    p = root / 'benchmarks/runs' / rel
    owned.append(str(p.relative_to(root)))
    summary = json.loads(p.read_text())
    for name, digest in summary['hashes'].items(): bind(root / name, digest)
proof = root / 'benchmarks/runs/20260910-7033-collect-retry-proofs'
assert not list(proof.rglob('*.cnf')) and not list(proof.rglob('*.proof'))
owned += [str(p.relative_to(root)) for p in sorted((proof / 'logs').iterdir())]
gate = root / 'benchmarks/runs/20260910-7034-collect-retry-BC-gate'
gm = json.loads((gate / 'manifest.json').read_text())
launch = json.loads((gate / 'launch.json').read_text())
assert launch['manifest_sha256'] == sha(gate / 'manifest.json')
for rel, digest in gm['pin_sha256'].items(): bind(gate / rel, digest)
for rel, digest in launch['protected_campaign_hashes'].items(): bind(root / rel, digest)
owned += [str((gate / name).relative_to(root)) for name in ('manifest.json', 'launch.json', 'matrix.tsv', 'array.sbatch')]
for rel in owned: bind(root / rel)
report = root / 'report-to-andrew/20260910-0902-collect-retry'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').stat().st_size > 1000
checks = {}
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in
    ('check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
     'check-default-strategy-config', 'check-proof-rule-sync')]
commands += [[sys.executable, 'benchmarks/probes/scratch/localize-collect-retry-7032.py', '--self-test']]
for command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=r.returncode, output=r.stdout + r.stderr)
    assert r.returncode == 0, (command, r.stdout, r.stderr)
scheduler = {}
for label, command in (
    ('queue', ['squeue', '-h', '-j', '1679645,1679667', '-o', '%i %T %M %N']),
    ('accounting', ['sacct', '-X', '-j', '1679645,1679663,1679667', '--starttime=2026-09-10T00:00:00', '--format=JobID,State,ExitCode,End', '-P'])):
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    scheduler[label] = r.stdout
campaigns = {}
for name, count in (('20260909-6743-reduce-watermark-ranks1-50', 50),
                    ('20260910-7002-largest-gap-release-profiles', 4),
                    ('20260910-7006-transitive-largest-gap-ablation', 8),
                    ('20260910-7034-collect-retry-BC-gate', 66)):
    c = root / 'benchmarks/runs' / name
    states = Counter(); pending = []
    for task in range(count):
        p = c / f'results/{task:03d}.json'
        final = p.exists()
        if not final: p = c / f'provisional/{task:03d}.json'
        record = json.loads(p.read_text()) if p.exists() else None
        state = classify(record, final)
        assert state['status'] in ('checked_UNSAT', 'checked_SAT', 'pending_validation', 'pending_solver'), (p, state)
        if state['status'] == 'pending_solver':
            assert name == '20260910-7034-collect-retry-BC-gate'
            assert '1679667' in scheduler['queue'], 'no live gate authority'
        if record: bind(p)
        states[state['status']] += 1
        if not state['checked']: pending.append(task)
    campaigns[name] = dict(states=dict(states), pending_tasks=pending)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
    owned_paths=owned, checks=checks, scheduler=scheduler, campaigns=campaigns,
    decision='Isolated general retry advice repair built/tested; B/C before Rank35 causal replay and three-wave31/35 timing. No native trace or runtime improvement claimed yet. Full50 target unmet.',
    cleanup='7033 removed3998 bytes of own generated fixed inputs/proofs after strong checks. No campaign/cache deletion; active checker scratch and fuzz artifacts untouched.',
    report='Local TeX/PDF compiled twice, unsent. No promotion or denied push/transfer retry.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps(campaigns, indent=2))
