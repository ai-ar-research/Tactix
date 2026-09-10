#!/usr/bin/env python3
"""Freeze cost-priority evidence, failed observers and published certificate states."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root/'benchmarks/reports/2026-09-10-inprocessing-cost-priority-audit-6818.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-10-rank21-bump-membership-audit-6808.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes', 'protected_campaign_hashes', 'andrew_report'):
    for rel, digest in prior[group].items():
        if rel != 'PROGRESS.md': assert sha(root/rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'benchmarks/reports/2026-09-10-inprocessing-cost-priority.md'}
for name in ('bounded_trace_config', 'inprocessing_cost'):
    owned.update('tools/bench/'+prefix+name+'.py' for prefix in ('', 'test_'))
for number in range(6810, 6820):
    owned.update(str(p.relative_to(root)) for p in (root/'benchmarks/probes/scratch').glob(f'*-{number}.py'))
def own(p):
    owned.add(str(p.relative_to(root)))
def bindings(hashes):
    for rel, digest in hashes.items(): assert sha(root/rel) == digest, rel
evidence = {}; states = {}; accounting = {}
def summary(p):
    record = json.loads(p.read_text())
    bindings(record.get('hashes', {}))
    evidence[str(p.relative_to(root))] = sha(p)
    own(p)
    return record
def account(c, count, failed_tasks=()):
    launch = json.loads((c/'launch.json').read_text())
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'],
               '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    stdout = subprocess.check_output(command, text=True)
    rows = stdout.splitlines()
    assert len(rows) == count, (c, stdout)
    for row in rows:
        identity, state, code, submitted, *_ = row.split('|')
        task = int(identity.rsplit('_', 1)[1])
        if task in failed_tasks:
            assert (state, code) == ('FAILED', '1:0'), row
        else:
            assert state in ('PENDING', 'RUNNING', 'COMPLETING', 'COMPLETED') and code == '0:0', row
        assert submitted >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)), command=command, stdout=stdout)

old = root/'benchmarks/runs/20260910-6804-rank21-analysis-admission'
account(old, 4, (0, 2))
summary(old/'configuration-6810.json')
summary(old/'terminal-6813.json')
for window in ('early', 'late'):
    records = []
    for arm in ('candidate', 'kissat'):
        p = old/window/'results'/f'{arm}.json'
        r = json.loads(p.read_text())
        assert r['exit_code'] == (124 if arm == 'candidate' else 0)
        assert r['scratch_deleted']
        own(p)
        records.append(dict(arm=arm, exit_code=r['exit_code'], scratch_deleted=True,
                            accepted_observer=False))
    states[str((old/window).relative_to(root))] = records
assert not (root/'benchmarks/runs/20260910-6812-rank21-control-endpoint').exists()
summary(root/'benchmarks/runs/20260910-6814-inprocessing-cost-census/summary.json')

profile = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles'
accepted = summary(profile/'solver-summary-6817.json')
assert set(accepted['arms']) == {'32/candidate', '32/kissat', '39/candidate', '39/kissat'}
for name, count in (
    ('20260909-6743-reduce-watermark-ranks1-50', 50),
    ('20260909-6787-rank38-watermark-profile', 2),
    ('20260910-6815-inprocessing-release-profiles', 4),
):
    c = root/'benchmarks/runs'/name
    account(c, count)
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c/rel) == digest, rel
        if c == profile and (c/rel).suffix in ('.py', '.patch', '.csv', '.sbatch'):
            own(c/rel)
    for group in ('dependencies', 'protected_campaign_hashes'):
        bindings(m.get(group, {}))
    current = {}
    for folder in ('provisional', 'results'):
        for p in (c/folder).glob('*.json'):
            r = json.loads(p.read_text())
            status = classify(r, folder == 'results')
            assert status['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, status)
            assert r['task'] == int(p.stem)
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            own(p)
            current[r['task']] = dict(rank=r['rank'], arm=r['arm'], status=status)
    assert len(current) == count
    if count == 50: assert all(r['status']['checked'] and r['status']['clean'] for r in current.values())
    states[str(c.relative_to(root))] = current
for name in ('manifest.json', 'launch.json', 'matrix.csv', 'array.sbatch', 'run.py'):
    p = profile/name
    if p.exists(): own(p)
for p in (profile/'profiles').glob('*.txt'): own(p)

partial = profile.with_name(profile.name+'-prepare-attempt1')
assert partial.is_dir() and not (partial/'manifest.json').exists() and not (partial/'launch.json').exists()
partial_inventory = {}
for p in sorted(partial.rglob('*')):
    if p.is_file():
        counterpart = profile/p.relative_to(partial)
        partial_inventory[str(p.relative_to(root))] = dict(bytes=p.stat().st_size, sha256=sha(p),
            successor=str(counterpart.relative_to(root)),
            successor_identical=counterpart.is_file() and sha(p) == sha(counterpart))

command = ['squeue', '-h', '-u', 'hwu', '-o', '%i|%j|%T|%P|%C|%m']
stdout = subprocess.check_output(command, text=True)
owners = {'1668242': 'user interactive allocation; untouched',
          '1678659': '6743 full50; terminal published proof state authoritative',
          '1678859': '6815 immutable four-arm inprocessing profiles/checkers'}
mapping = {}
for line in stdout.splitlines():
    identity = line.split('|')[0]
    job = identity.split('_')[0]
    assert job in owners, ('unmapped active job', line)
    mapping[identity] = dict(row=line, owner=owners[job])
accounting['active_queue'] = dict(command=command, stdout=stdout, mapping=mapping)

checks = {}
commands = [(name, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/'+name, '-p', 'test_*.py'])
            for name in ('bench', 'slurm')]
commands += [(name, ['python3', 'tools/docs/'+name+'.py']) for name in
    ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync',
     'check-section-refs', 'check-citations')]
for name, command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=r.stdout, stderr=r.stderr)
    print(name, 'PASS', flush=True)
report = root/'report-to-andrew/20260910-0032-inprocessing-cost'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], evidence=evidence,
    campaign_states=states, accounting=accounting, checks=checks,
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex', 'report.pdf')},
    retained_failed_preparation=partial_inventory,
    profile_interpretation='Validated6817 exact own-arm work/proof and zero-loss samples. Rank32 current Vivify10.70% inclusive, schedule2.99%; Rank39 Sweep16.08% inclusive with SubSolver propagation7.44% self. Inspect candidate construction/watch rebuilding and Sweep environment/subsolver as general same-work cost targets. These percentages are not operation prices or global scheduling/CDCL parity.',
    cleanup='No manual deletion. Failed trace scratch clean; full50 all checked-clean. Existing checkers own their scratch; no live scratch or fuzz artifacts inspected. Large original profiles/traces remain hash-bound locally. Failed unsubmitted6815 scaffold retained explicitly.',
    limitations='No solver change, promotion, speedup, reference refresh, FlatLists redesign or denied transfer/push retry.6730 isolated/unfuzzed,full50 ratio1.146588277 and memory parity unmet. Rank21 probes shelved; accepted queue frontier open. Ranks31/35 changed-work gaps remain. Andrew report local/unsent. Winddown05:45/cutoff06:00EDT September10.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(target)
for name, records in states.items():
    if isinstance(records, dict):
        print(name, len(records), 'published', sum(r['status']['checked'] and r['status']['clean'] for r in records.values()), 'checked/clean')
