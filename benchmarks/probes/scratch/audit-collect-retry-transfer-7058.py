#!/usr/bin/env python3
"""Freeze the native closure, repeated tradeoff and isolated transfer launch."""
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
target = root / 'benchmarks/reports/2026-09-10-collect-retry-transfer-audit-7058.json'
assert not target.exists()
hashes = {}
owned = ['PROGRESS.md', 'benchmarks/reports/2026-09-10-collect-retry-native-closure.md']
def bind(p, expected=None):
    value = sha(p)
    assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
helpers = (
    'prepare-collect-retry-prefix-7038', 'submit-collect-retry-prefix-7039',
    'validate-collect-retry-prefix-7040', 'prepare-collect-retry-runtime-7041',
    'submit-collect-retry-wave-7042', 'classify-collect-retry-BC-work-7043',
    'summarize-collect-retry-waves-7044', 'localize-collect-retry-boundary-7045',
    'prepare-collect-retry-rank31-7046', 'submit-collect-retry-rank31-7047',
    'validate-collect-retry-rank31-7048', 'prepare-collect-retry-rank31-late-7050',
    'submit-collect-retry-rank31-late-7051', 'validate-collect-retry-rank31-late-7052',
    'prepare-collect-retry-full50-7053', 'submit-collect-retry-full50-7054',
    'prepare-collect-retry-rank31-2m-7055', 'submit-collect-retry-rank31-2m-7056',
    'validate-collect-retry-rank31-2m-7057', 'audit-collect-retry-transfer-7058')
owned += ['benchmarks/probes/scratch/' + name + '.py' for name in helpers]
build = root / 'benchmarks/runs/20260910-7030-collect-retry-build'
bm = json.loads((build / 'manifest.json').read_text())
bind(build / 'manifest.json')
for rel, digest in bm['files'].items(): bind(root / bm['source'] / rel, digest)
for rel, digest in bm['binaries'].items(): bind(root / rel, digest)
jobs = '1679667,1679734,1679735,1679740,1679745,1679748,1679752,1679755,1679779'
scheduler = {}
for name, command in (
    ('queue', ['squeue', '-h', '-r', '-j', jobs, '-o', '%i|%T']),
    ('accounting', ['sacct', '-n', '-X', '--array', '-j', jobs, '-S', '2026-09-10T08:58:00', '-o', 'JobID,State%40,ExitCode,Submit', '-P'])):
    r = subprocess.run(command, capture_output=True, text=True, check=True)
    scheduler[name] = r.stdout
states = {}
for line in scheduler['accounting'].splitlines():
    fields = line.split('|')
    if fields[0]: states[fields[0].strip()] = fields[1].strip()
for line in scheduler['queue'].splitlines():
    key, value = line.split('|'); states[key.strip()] = value.strip()
campaigns = {}
specs = [
    ('20260910-7034-collect-retry-BC-gate', 66, lambda t: f'1679667_{t}'),
    ('20260910-7041-collect-retry-runtime', 12, lambda t: f'{(1679735,1679740,1679748)[t//4]}_{t}'),
    ('20260910-7053-collect-retry-ranks1-50', 50, lambda t: f'1679755_{t}')]
for name, count, identity in specs:
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    launches = sorted(c.glob('*submission.json')) if count == 12 else [c / 'launch.json']
    for p in launches:
        launch = json.loads(p.read_text()); bind(p)
        assert launch['manifest_sha256'] == sha(c / 'manifest.json')
        for rel, digest in launch.get('protected_campaign_hashes', {}).items(): bind(root / rel, digest)
    counts = Counter(); pending = []; records = []
    for task in range(count):
        p = c / f'results/{task:03d}.json'; final = p.exists()
        if not final: p = c / f'provisional/{task:03d}.json'
        record = json.loads(p.read_text()) if p.exists() else None
        assert identity(task) in states, identity(task)
        state = classify(record, final, states[identity(task)])
        assert state['status'] in ('checked_SAT', 'checked_UNSAT', 'pending_validation', 'pending_solver'), (p, state)
        if record:
            assert int(record['task']) == task
            if final: assert record['binary_sha256'] == m['arms'][record['arm']]['sha256']
            bind(p); owned.append(str(p.relative_to(root)))
        counts[state['status']] += 1
        if not state['checked']: pending.append(task)
        records.append(dict(task=task, state=state, scheduler_state=states[identity(task)]))
    campaigns[name] = dict(states=dict(counts), pending_tasks=pending, tasks=records)

diagnostics = {}
for name, arms, job in (
    ('20260910-7038-collect-retry-prefix', ('candidate',), 1679734),
    ('20260910-7046-collect-retry-rank31', ('candidate','previous'), 1679745),
    ('20260910-7050-collect-retry-rank31-late', ('candidate','previous'), 1679752),
    ('20260910-7055-collect-retry-rank31-2m', ('candidate','previous'), 1679779)):
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m['hashes'].items(): bind(c / rel, digest)
    launch = json.loads((c / 'launch.json').read_text())
    assert launch['manifest_sha256'] == sha(c / 'manifest.json')
    arms_out = {}
    for task, arm in enumerate(arms):
        p = c / 'results' / (arm + '.json')
        if p.exists():
            r = json.loads(p.read_text())
            assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
            assert r['binary_sha256'] == m['hashes']['pins/' + arm]
            bind(p); owned.append(str(p.relative_to(root)))
            arms_out[arm] = dict(terminal_clean=True, partial_certificate=r['partial_certificate'])
        else:
            assert states.get(f'{job}_{task}') in ('PENDING','RUNNING','COMPLETING','CONFIGURING')
            arms_out[arm] = dict(terminal_clean=False, live=states[f'{job}_{task}'])
    diagnostics[name] = arms_out

dirs = [name for name, _, _ in specs] + list(diagnostics)
for name in dirs:
    c = root / 'benchmarks/runs' / name
    owned += [str(p.relative_to(root)) for p in sorted(c.iterdir())
              if p.is_file() and p.suffix in ('.json','.py','.sbatch','.tsv')]
    owned += [str(p.relative_to(root)) for p in sorted((c / 'pins').iterdir())
              if p.suffix in ('.py','.patch')]
late = root / 'benchmarks/runs/20260910-7050-collect-retry-rank31-late'
for suffix in ('macro.tsv', 'boundaries.tsv'):
    a, b = (late / 'traces' / (arm + '.' + suffix) for arm in ('candidate','previous'))
    bind(a); bind(b); assert sha(a) == sha(b)
for rel in sorted(set(owned)): bind(root / rel)
report = root / 'report-to-andrew/20260910-0950-runtime-transfer'
for name in ('report.tex', 'report.pdf', 'report.log'): bind(report / name)
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', d, '-p', 'test_*.py'] for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + n + '.py'] for n in ('check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary','check-default-strategy-config','check-proof-rule-sync')]
commands += [[sys.executable, 'benchmarks/probes/scratch/submit-collect-retry-wave-7042.py', '--self-test']]
checks = {}
for command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True)
    checks[' '.join(command)] = dict(exit_code=r.returncode, output=r.stdout + r.stderr)
    assert r.returncode == 0, (command, r.stdout, r.stderr)
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
    owned_paths=sorted(set(owned)), scheduler=scheduler, campaigns=campaigns, diagnostics=diagnostics, checks=checks,
    previous_turn='Status-only answer: no progress. Revalidated state, completed7052 and submitted7053/7055 in this continuation.',
    decision='Rank35 repeated29.2% gain; Rank31 repeated9.8% adverse transfer begins after1M. Continue from isolated7030 and localize the next coupled mechanism. Full50 measurement is not promotion; goal unmet.',
    cleanup='Completed stopped tasks publish partial-proof hashes and deleted scratch; no manual material deletion this turn. Active checker scratch, caches and fuzz artifacts untouched. Remaining certificate/cleanup obligations are explicitly pending.',
    report='Local TeX/PDF compiled twice with cluster TeX module; no Overfull boxes. Unsent. No promotion, fuzz access or denied push/transfer retry.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps({k:v['states'] for k,v in campaigns.items()}, indent=2))
