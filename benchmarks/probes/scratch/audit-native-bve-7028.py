#!/usr/bin/env python3
"""Freeze completed diagnostics and classify asynchronous checks without waiting."""
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

target = root / 'benchmarks/reports/2026-09-10-native-bve-audit-7028.json'
assert not target.exists(), 'successful audit frozen'
hashes = {}
def bind(path, expected=None):
    actual = sha(path)
    assert expected is None or actual == expected, path
    hashes[str(path.relative_to(root))] = actual

for rel in ('20260910-7002-largest-gap-release-profiles',
            '20260910-7006-transitive-largest-gap-ablation',
            '20260910-7011-rank35-current-native-prefix',
            '20260910-7018-rank35-bve-frontier',
            '20260910-7024-rank35-retry-producer'):
    c = root / 'benchmarks/runs' / rel
    m = json.loads((c / 'manifest.json').read_text())
    for name in ('manifest.json', 'launch.json'):
        bind(c / name)
    for name, digest in m.get('pin_sha256', m.get('hashes', {})).items():
        bind(c / name, digest)
    for group in ('dependencies', 'protected_campaign_hashes'):
        for name, digest in m.get(group, {}).items():
            bind(root / name, digest)
    for item in m.get('unprofiled_records', {}).values():
        bind(root / item['path'], item['sha256'])
for rel in ('20260910-7002-largest-gap-release-profiles/solver-summary-7004.json',
            '20260910-7002-largest-gap-release-profiles/cost-priority-7010.json',
            '20260910-7006-transitive-largest-gap-ablation/solver-summary-7008.json',
            '20260910-7011-rank35-current-native-prefix/summary-7015.json',
            '20260910-7011-rank35-current-native-prefix/boundary-localization-7017.json',
            '20260910-7018-rank35-bve-frontier/schedule-summary-7020.json',
            '20260910-7018-rank35-bve-frontier/candidate-summary-7021.json',
            '20260910-7018-rank35-bve-frontier/attempt-alignment-7023.json'):
    p = root / 'benchmarks/runs' / rel
    bind(p)
    for name, digest in json.loads(p.read_text())['hashes'].items():
        bind(root / name, digest)

owned = ['PROGRESS.md', 'benchmarks/reports/2026-09-10-rank35-native-bve-boundary.md']
owned += ['benchmarks/probes/scratch/' + name + '.py' for name in
          ('validate-rank35-native-prefix-7015', 'localize-rank35-native-boundary-7016',
           'localize-rank35-native-boundary-7017', 'prepare-rank35-bve-frontier-7018',
           'submit-rank35-bve-frontier-7019', 'validate-rank35-bve-schedule-7020',
           'validate-rank35-bve-candidates-7021', 'align-rank35-bve-attempts-7023',
           'prepare-rank35-retry-producer-7024', 'submit-rank35-retry-producer-7025',
           'validate-rank35-retry-producer-7026', 'audit-native-bve-7027', 'audit-native-bve-7028')]
for rel in owned:
    bind(root / rel)
report = root / 'report-to-andrew/20260910-0840-native-bve'
for name in ('report.tex', 'report.pdf', 'report.log'):
    bind(report / name)
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').stat().st_size > 1000

checks = {}
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', directory, '-p', 'test_*.py']
            for directory in ('tools/bench', 'tools/slurm')]
commands += [[sys.executable, 'tools/docs/' + name + '.py'] for name in
             ('check-doc-size', 'check-progress', 'check-section-refs',
              'check-rule-policy-boundary', 'check-default-strategy-config', 'check-proof-rule-sync')]
for command in commands:
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    checks[' '.join(command)] = dict(exit_code=result.returncode, output=result.stdout + result.stderr)
    assert result.returncode == 0, (command, result.stdout, result.stderr)

campaigns = {}
for rel, count in (('20260909-6743-reduce-watermark-ranks1-50', 50),
                   ('20260910-6980-xor-presence-ranks1-50', 50),
                   ('20260910-6991-ite-pair-BC-gate', 66),
                   ('20260910-6994-ite-pair-runtime', 18),
                   ('20260910-7002-largest-gap-release-profiles', 4),
                   ('20260910-7006-transitive-largest-gap-ablation', 8)):
    c = root / 'benchmarks/runs' / rel
    states, pending = Counter(), []
    for task in range(count):
        p = c / f'results/{task:03d}.json'
        if not p.exists(): p = c / f'provisional/{task:03d}.json'
        r = json.loads(p.read_text()) if p.exists() else None
        state = classify(r, p.parent.name == 'results')
        assert state['status'] in ('checked_UNSAT', 'checked_SAT', 'pending_validation'), (p, state)
        states[state['status']] += 1
        if r: bind(p)
        if not state['checked']: pending.append(task)
    campaigns[rel] = dict(states=dict(states), pending_tasks=pending)
assert not campaigns['20260910-6980-xor-presence-ranks1-50']['pending_tasks']
observations = {}
for name, command in (
    ('queue', ['squeue', '-j', '1679638,1679645,1679663', '-o', '%.20i %.12T %.10M %.10l %.18R']),
    ('accounting', ['sacct', '-X', '-j', '1679638,1679645,1679655,1679658,1679663', '--starttime=2026-09-10T00:00:00', '--format=JobID,State,ExitCode,End', '-P'])):
    result = subprocess.run(command, cwd=root, text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    observations[name] = result.stdout
diagnostics = {}
for rel, arms in (
    ('20260910-7011-rank35-current-native-prefix', ('candidate', 'kissat')),
    ('20260910-7018-rank35-bve-frontier', ('candidate', 'kissat', 'candidate_schedule', 'kissat_schedule')),
    ('20260910-7024-rank35-retry-producer', ('candidate_schedule', 'kissat_schedule'))):
    c = root / 'benchmarks/runs' / rel
    tasks = {}
    for arm in arms:
        p = c / 'results' / (arm + '.json')
        if not p.exists():
            tasks[arm] = 'pending'; continue
        r = json.loads(p.read_text()); bind(p)
        assert r['exit_code'] == 0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
        for name, digest in r['trace_sha256'].items(): bind(c / 'traces' / name, digest)
        tasks[arm] = 'clean_stopped_diagnostic_not_final_proof'
    diagnostics[rel] = tasks
out = dict(prior_audit_failure='7027 correctly rejected PROGRESS shorthand L140 as a dangling lesson citation. Status now spells out level140; no checker weakened. Failed helper preserved; no audit output had been written.', diagnostics=diagnostics, observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes,
           owned_paths=owned, checks=checks, campaigns=campaigns, scheduler=observations,
           decision='Largest-work lead:21 extra current BVE members with exact shared scores;196 common attempts then order split. Validate7024 own replay,then trace174970 retry provenance. No solver/default change;989s aggregate target deficit remains.',
           cleanup='No manual deletion; final records enforce certificate/derivative cleanup. Active checker scratch and fuzz artifacts not inspected.',
           session='Explicit user resumed after old cutoff; replacement cutoff requested/pending.',
           report='Local and unsent; no denied push/transfer retry.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
print(json.dumps(campaigns, indent=2))
