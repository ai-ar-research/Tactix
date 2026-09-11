#!/usr/bin/env python3
"""Freeze a non-promoting telemetry investigation and full-set handoff audit."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-10-runtime-telemetry-checkpoint-7177.json'
assert not target.exists()
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    digest = sha(p)
    assert expected is None or digest == expected, p
    rel = str(p.relative_to(root)); hashes[rel] = digest
    if own: owned.add(rel)
assert not subprocess.check_output(['git', 'diff', 'c2a95a6', '--', 'src', 'include'], cwd=root)
old_path = root / 'benchmarks/reports/2026-09-10-vivify-revalidation-audit-7169.json'
old = json.loads(old_path.read_text()); bind(old_path, own=True)
for rel in old['owned_paths']:
    bind(root / rel, None if rel == 'PROGRESS.md' else old['hashes'][rel], True)
for rel in ('PROGRESS.md', 'DEVELOPMENT_PLAN.md', '.claude/skills/revamp/SKILL.md'):
    bind(root / rel, own=rel == 'PROGRESS.md')
for name in ('localize-factor-pcs-7170', 'localize-factor-envelope-7171',
             'build-runtime-telemetry-7172', 'report-runtime-evaluation-7173',
             'runtime-telemetry-contract-7174', 'prepare-runtime-telemetry-7174',
             'check-submit-runtime-telemetry-7175', 'check-runtime-telemetry-sanitizers-7176',
             'audit-runtime-telemetry-7177'):
    bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
for rel in ('benchmarks/runs/20260910-7170-factor-pc-localization/summary.json',
            'benchmarks/reports/2026-09-10-factor-envelope-7171.json',
            'benchmarks/reports/2026-09-10-runtime-evaluation-audit-7173.md',
            'benchmarks/reports/2026-09-10-vivify-revalidation-full50-7168/summary.json',
            'benchmarks/reports/2026-09-10-vivify-revalidation-full50-7168/comparison.tsv'):
    bind(root / rel, own=True)
authority = root / 'benchmarks/runs/20260910-7172-runtime-telemetry-build'
m = json.loads((authority / 'manifest.json').read_text())
for rel, digest in m['files'].items(): bind(root / m['source'] / rel, digest)
for rel, digest in m['binaries'].items(): bind(root / rel, digest)
assert m['binaries']['build/pin-7172-runtime-telemetry-control/tactix'] == sha(root / 'build/pin-7162-vivify-revalidation-release/tactix')
for p in authority.rglob('*'):
    if p.is_file(): bind(p, own=True)
san = root / 'benchmarks/runs/20260910-7176-runtime-telemetry-sanitizers'
g = json.loads((san / 'summary.json').read_text()); assert g['status'] == 'PASS'
for rel, digest in g['hashes'].items(): bind(root / rel, digest)
for p in san.iterdir():
    if p.suffix in ('.log', '.json'): bind(p, own=True)
specs = [('20260910-7166-vivify-revalidation-BC-gate', 66, None),
         ('20260910-7167-vivify-revalidation-ranks1-50', 50, None),
         ('20260910-7150-vivify-compact-runtime', 18, 6),
         ('20260910-7152-vivify-compact-profiles', 6, 2),
         ('20260910-7174-runtime-telemetry-BC', 66, None),
         ('20260910-7175-runtime-telemetry-runtime', 18, 6)]
jobs, launches = [], {}
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text()); bind(c / 'manifest.json', own=True)
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    launches[name] = {}
    for wave in range(3 if width else 1):
        p = c / (f'wave{wave}-submission.json' if width else 'launch.json')
        if not p.exists():
            assert name == '20260910-7175-runtime-telemetry-runtime' and wave > 0
            continue
        launch = json.loads(p.read_text()); bind(p, own=True)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        for rel, digest in launch.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
        for task in (range(width*wave, width*(wave+1)) if width else range(count)):
            launches[name][task] = launch['job_id']
        jobs.append(launch['job_id'])
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch'): bind(p, own=True)
    for p in (c / 'pins').iterdir():
        if p.suffix in ('.py', '.patch'): bind(p, own=True)
queue = subprocess.check_output(['squeue', '-h', '-r', '-u', 'hwu', '-o', '%i|%T|%j'], text=True)
accounting = subprocess.check_output(['sacct', '-n', '-X', '--array', '-j', ','.join(jobs),
    '-S', '2026-09-10T21:26:00', '-o', 'JobID,State%40,ExitCode,Start', '-P'], text=True)
states = {parts[0]: parts[1].strip() for line in accounting.splitlines()
          if len(parts := line.split('|')) >= 2 and parts[0]}
for line in queue.splitlines():
    task, state, _ = line.split('|'); states[task] = state
campaigns = {}
for name, count, width in specs:
    c = root / 'benchmarks/runs' / name
    records = []
    for task, job in launches[name].items():
        identity = f'{job}_{task}'; assert identity in states, identity
        p = c / f'results/{task:03d}.json'; final = p.exists()
        if not final: p = c / f'provisional/{task:03d}.json'
        r = json.loads(p.read_text()) if p.exists() else None
        state = classify(r, final, states[identity])
        assert state['status'] in ('pending_solver', 'pending_validation', 'checked_SAT', 'checked_UNSAT'), (identity, state)
        if r: bind(p, own=True)
        records.append(dict(task=task, scheduler_state=states[identity], classification=state))
    campaigns[name] = records
report = root / 'report-to-andrew/20260910-2257-runtime-evaluation'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync', 'check-option-coverage')]
commands += [[sys.executable, 'benchmarks/probes/scratch/' + name + '.py'] for name in (
    'validate-vivify-revalidation-BC-7166', 'validate-vivify-revalidation-full50-7168',
    'validate-vivify-compact-profiles-7152', 'runtime-telemetry-contract-7174')]
commands += [[sys.executable, 'benchmarks/probes/scratch/check-submit-runtime-telemetry-7175.py', kind] for kind in ('bc', 'runtime')]
commands += [[sys.executable, 'benchmarks/probes/scratch/localize-factor-envelope-7171.py', '--self-test']]
checks = {}
for command in commands:
    p = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode, output=p.stdout+p.stderr)
    assert p.returncode == 0, (command, p.stdout, p.stderr)
assembly = {}
for kind in ('control', 'release'):
    output = subprocess.check_output(['nm', '-S', '-C', str(root / f'build/pin-7172-runtime-telemetry-{kind}/tactix')], text=True)
    assembly[kind] = [line for line in output.splitlines() if any(line.endswith(symbol) for symbol in (
        'tactix::apply_rule(tactix::RuleVTable const&, tactix::Kernel&)',
        'tactix::kernel_propagate(tactix::Kernel&, unsigned int)',
        'tactix::kernel_propagate_binary(tactix::Kernel&, bool)'))]
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
        owned_paths=sorted(owned), campaigns=campaigns, queue=queue, accounting=accounting,
        checks=checks, assembly=assembly, newly_deleted_bytes=0,
        decision='Fresh full50 solver-phase revalidation complete, exact work/proofs and provisional ratio1.115077752x. Retain7144; target remains unmet. Isolated7172 optional telemetry ablation passes full suites/sanitizers and BC solver neutrality, first targeted wave launched without proof wait. No new default/production change, fuzz acceptance or denied transfer/push retry.',
        limitation='Binary-scan/clock ablation also displaces downstream code; same-size search propagation moves336 bytes. Joint runtime comparison is not a pure price of counter instructions. All final proof/cleanup, memory, current-source fuzz and consolidation gates remain separate.',
        next_action='Validate7175 wave0 with strict telemetry contract; advance waves1/2 only after exact expected own/cross-arm solver outcomes, never wait for proofs. Preserve three paired ranges/RSS and compare against larger Factor lead7171. Successful writers and submitted pins remain immutable.'), out, indent=2)
print(target)
for name, rows in campaigns.items(): print(name, sum(r['classification']['checked'] for r in rows), '/', len(rows), 'checked-clean')
