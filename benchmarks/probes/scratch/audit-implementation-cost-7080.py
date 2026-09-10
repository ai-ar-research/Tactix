#!/usr/bin/env python3
"""Freeze current authority, checks and cost-directed handoff without new jobs."""
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
target = root / 'benchmarks/reports/2026-09-10-implementation-cost-audit-7080.json'
assert not target.exists()
hashes = {}
owned = ['PROGRESS.md', 'tools/bench/native_work.py', 'tools/bench/test_native_work.py',
         'tools/bench/perf_symbol_cost.py', 'tools/bench/test_perf_symbol_cost.py',
         'benchmarks/reports/2026-09-10-implementation-cost-direction.md',
         'benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json',
         'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json']
owned += ['benchmarks/probes/scratch/' + name + '.py' for name in (
    'prepare-rank43-full-decisions-7077', 'submit-rank43-full-decisions-7078',
    'summarize-rank35-cost-scopes-7079', 'audit-implementation-cost-7080')]
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest

specs = [
    ('20260910-7034-collect-retry-BC-gate', 66, lambda t: f'1679667_{t}'),
    ('20260910-7041-collect-retry-runtime', 12, lambda t: f'{(1679735,1679740,1679748)[t//4]}_{t}'),
    ('20260910-7053-collect-retry-ranks1-50', 50, lambda t: f'1679755_{t}'),
    ('20260910-7060-rank35-residual-profile', 1, lambda t: f'1679809_{t}'),
    ('20260910-7064-rank31-full-boundaries', 2, lambda t: f'1679811_{t}'),
    ('20260910-7068-rank31-selected-bve', 2, lambda t: f'1679815_{t}'),
    ('20260910-7073-rank31-bve-priority', 2, lambda t: f'1679819_{t}')]
jobs = '1679667,1679735,1679740,1679748,1679755,1679809,1679811,1679815,1679819'
scheduler = {}
for name, command in (
    ('queue', ['squeue', '-h', '-r', '-j', jobs, '-o', '%i|%T']),
    ('all_user_queue', ['squeue', '-h', '-r', '-u', 'hwu', '-o', '%i|%T|%j']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', jobs, '-S', '2026-09-10T08:58:00',
                    '-o', 'JobID,State%40,ExitCode,Submit', '-P'])):
    scheduler[name] = subprocess.check_output(command, text=True)
states = {}
for line in scheduler['accounting'].splitlines():
    f = line.split('|')
    if f[0]: states[f[0].strip()] = f[1].strip()
for line in scheduler['queue'].splitlines():
    key, value = line.split('|'); states[key.strip()] = value.strip()
campaigns = {}
for name, count, identity in specs:
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text()); bind(c / 'manifest.json')
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for rel, digest in m.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
    launches = sorted(c.glob('*submission.json')) if count == 12 else [c / 'launch.json']
    for p in launches:
        bind(p); launch = json.loads(p.read_text())
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
    counts = Counter(); tasks = []
    for task in range(count):
        p = c / f'results/{task:03d}.json'; final = p.exists()
        if not final: p = c / f'provisional/{task:03d}.json'
        r = json.loads(p.read_text()) if p.exists() else None
        assert identity(task) in states, identity(task)
        state = classify(r, final, states[identity(task)])
        assert state['status'] in ('checked_SAT', 'checked_UNSAT', 'pending_validation', 'pending_solver'), (p, state)
        if r:
            assert r['task'] == task
            if final: assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            bind(p); owned.append(str(p.relative_to(root)))
        counts[state['status']] += 1
        tasks.append(dict(task=task, state=state, scheduler_state=states[identity(task)]))
    campaigns[name] = dict(states=dict(counts), tasks=tasks)

for name in ('20260910-7030-collect-retry-build', '20260909-6730-reduce-watermark-build'):
    p = root / 'benchmarks/runs' / name / 'manifest.json'; bind(p)
    m = json.loads(p.read_text())
    for rel, digest in m['files'].items(): bind(root / m['source'] / rel, digest)
    for rel, digest in m['binaries'].items(): bind(root / rel, digest)
for rel in ('benchmarks/reports/2026-09-10-rank35-cost-scopes-7079.json',
            'benchmarks/runs/20260910-7073-rank31-bve-priority/solver-summary-7075.json'):
    p = root / rel; bind(p)
    for source, digest in json.loads(p.read_text())['hashes'].items(): bind(root / source, digest)
prepared = root / 'benchmarks/runs/20260910-7077-rank43-full-decisions'
assert not (prepared / 'launch.json').exists()
assert not list((prepared / 'results').iterdir()) and not list((prepared / 'provisional').iterdir())
pm = json.loads((prepared / 'manifest.json').read_text())
for rel, digest in pm['pin_sha256'].items(): bind(prepared / rel, digest)
for group in ('dependencies', 'protected_campaign_hashes'):
    for rel, digest in pm[group].items(): bind(root / rel, digest)
owned += [str(p.relative_to(root)) for p in prepared.iterdir()
          if p.is_file() and p.suffix in ('.json', '.tsv', '.sbatch')]
owned += [str(p.relative_to(root)) for p in (prepared / 'pins').iterdir() if p.suffix == '.py']
for rel in ('kissat/src/resolve.c', 'kissat/src/eliminate.c',
            '.claude/worktrees/runtime-collect-retry-7030/src/rules/bve/bve.cpp'):
    bind(root / rel)
report = root / 'report-to-andrew/20260910-1115-implementation-cost'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py']
            for d in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in (
    'check-doc-size', 'check-progress', 'check-section-refs', 'check-rule-policy-boundary',
    'check-default-strategy-config', 'check-proof-rule-sync')]
commands += [[sys.executable, 'benchmarks/probes/scratch/validate-rank31-bve-priority-7075.py', '--self-test']]
checks = {}
for command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=r.returncode, output=r.stdout + r.stderr)
    assert r.returncode == 0, (command, r.stdout, r.stderr)
for rel in sorted(set(owned)): bind(root / rel)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
    owned_paths=sorted(set(owned)), scheduler=scheduler, campaigns=campaigns, checks=checks,
    previous_turn='Status-only response was no progress. Current continuation validates7075 and derives7079 cost envelopes, changing the next action toward broad inprocessing implementation efficiency.',
    decision='No new jobs/source/defaults. Audit common-scope cost and scheduling strength before another leaf optimization.7077 remains preserved/unsubmitted. Runtime/memory/current-source fuzz goals unmet.',
    cleanup='No material manual deletion. Published checked/clean records attest their campaign residue; active checks are not accepted. No active scratch or fuzz artifact access. Prepared7077 is a preserved secondary diagnostic, not abandoned; no deletion attempted.',
    report='Local TeX/PDF compiled twice, no Overfull; unsent. Existing doc-size debt reported, not compacted. Prior reports/pins unchanged. Denied push/transfer not retried.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps({k:v['states'] for k,v in campaigns.items()}, indent=2))
