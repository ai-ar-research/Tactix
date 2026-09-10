#!/usr/bin/env python3
"""Audit the isolated snapshot experiment and unchanged live campaign authorities."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-10-vivify-snapshot-audit-6836.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-10-inprocessing-local-profiles-6821.json'
prior = json.loads(prior_path.read_text())
for rel, digest in prior['owned_hashes'].items():
    if rel != 'PROGRESS.md': assert sha(root/rel) == digest, rel
for rel, record in prior['local_raw_profiles'].items(): assert sha(root/rel) == record['sha256']
previous_audit = json.loads((root/prior['audit']['path']).read_text())
assert sha(root/prior['audit']['path']) == prior['audit']['sha256']
protected = previous_audit['protected_campaign_hashes']
for group in (protected, previous_audit['andrew_report']):
    for rel, digest in group.items(): assert sha(root/rel) == digest, rel
assert not subprocess.check_output(['git', 'diff', '--', 'src', 'include', 'CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md', 'docs/scheduling-parity-ledger.md', 'benchmarks/reports/2026-09-10-vivify-snapshot-cost.md'}
for number in range(6822, 6838):
    owned.update(str(p.relative_to(root)) for p in (root/'benchmarks/probes/scratch').glob(f'*-{number}.py'))
def own(p): owned.add(str(p.relative_to(root)))
def bind(hashes):
    for rel, digest in hashes.items(): assert sha(root/rel) == digest, rel
evidence = {}; states = {}; accounting = {}
def summary(p):
    r = json.loads(p.read_text()); bind(r.get('hashes', {}))
    evidence[str(p.relative_to(root))] = sha(p); own(p)
    return r
build = root/'benchmarks/runs/20260910-6822-vivify-snapshot-build'
b = json.loads((build/'manifest.json').read_text())
for rel, digest in b['files'].items(): assert sha(root/b['source']/rel) == digest
bind(b['binaries'])
for rel, digest in b['patches'].items(): assert sha(build/rel) == digest; own(build/rel)
for rel, digest in b['logs'].items(): assert sha(build/rel) == digest
for name in ('manifest.json', 'parent.json', 'failed-test_vivify_snapshot.cpp', 'failed-test_vivify_count_cache.cpp'):
    own(build/name)
failure_path = build/'fixture-failure-6825.json'
failure = json.loads(failure_path.read_text())
changed_helper = b['source']+'/tests/test_vivify_count_cache.cpp'
for rel, digest in failure['hashes'].items():
    path = build/'failed-test_vivify_count_cache.cpp' if rel == changed_helper else root/rel
    assert sha(path) == digest, rel
own(failure_path)
for p in (build/'logs').glob('*.json'): own(p)
summary(build/'scope-6834.json')
control = root/'benchmarks/runs/20260910-6824-vivify-snapshot-control'
r = summary(control/'control-6824.json')
for group in ('logs', 'fixtures', 'parent_objects'): bind(r[group])
for p in (control/'logs').glob('*.json'): own(p)
proof = root/'benchmarks/runs/20260910-6827-vivify-snapshot-proofs'
r = summary(proof/'summary.json')
assert r['checked'] == 48 and r['removed_bytes'] == 65799
assert not list(proof.glob('*.cnf')) and not list(proof.glob('*.proof'))
for p in proof.glob('*-fixture.json'): own(p)

def account(c, launch_path, count):
    launch = json.loads(launch_path.read_text())
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct', '-n', '-X', '--array', '-j', launch['job_id'], '-S', launch['accounting_since'], '-o', 'JobID,State%40,ExitCode,Submit', '-P']
    stdout = subprocess.check_output(command, text=True)
    rows = stdout.splitlines(); assert len(rows) == count, (c, stdout)
    for row in rows:
        fields = row.split('|')
        assert fields[1] in ('PENDING', 'RUNNING', 'COMPLETING', 'COMPLETED') and fields[2] == '0:0', row
        assert fields[3] >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)), command=command, stdout=stdout)
    own(launch_path)

for name, count, required in (
    ('20260910-6828-vivify-snapshot-BC-gate', 66, 66),
    ('20260910-6815-inprocessing-release-profiles', 4, 4),
    ('20260910-6831-vivify-snapshot-runtime', 18, 0),
):
    c = root/'benchmarks/runs'/name
    m = json.loads((c/'manifest.json').read_text())
    for rel, digest in m['pin_sha256'].items():
        assert sha(c/rel) == digest, rel
        if (c/rel).suffix in ('.py', '.patch', '.tsv', '.sbatch'): own(c/rel)
    if isinstance(m.get('dependencies'), dict): bind(m['dependencies'])
    if c.name.endswith('-runtime'):
        for p in sorted(c.glob('wave*-submission.json')): account(c, p, 6)
    else: account(c, c/'launch.json', count)
    own(c/'manifest.json')
    current = {}
    for folder in ('provisional', 'results'):
        for p in (c/folder).glob('*.json'):
            r = json.loads(p.read_text()); state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
            assert r['task'] == int(p.stem) and 0 <= r['task'] < count
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            if 'work_oracles' in m:
                binding = m['work_oracles'][r['arm']][str(r['rank'])]
                assert sha(root/binding['path']) == binding['sha256']
                old = json.loads((root/binding['path']).read_text())
                for key in ('certificate_sha256', 'certificate_bytes', 'verdict', 'instance'): assert r[key] == old[key], (p, key)
                nt = lambda x: [s for s in x['solver_statistics'] if not s.startswith('c timing')]
                assert nt(r) == nt(old), p
            own(p); current[r['task']] = dict(rank=r['rank'], arm=r['arm'], state=state)
    assert len(current) >= required
    states[str(c.relative_to(root))] = current
g = summary(root/'benchmarks/runs/20260910-6828-vivify-snapshot-BC-gate/summary-6830.json')
assert len(g['pairs']) == 33 and all(p['reported_work_equal'] and p['certificate_equal'] for p in g['pairs'])
summary(root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/solver-summary-6817.json')
full = root/'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50'
full_states = {}
for task in range(50):
    p = full/f'results/{task:03d}.json'; r = json.loads(p.read_text()); status = classify(r, True)
    assert status['checked'] and status['clean']; full_states[task] = status
states[str(full.relative_to(root))] = full_states

command = ['squeue', '-h', '-u', 'hwu', '-o', '%i|%j|%T|%P|%C|%m']
stdout = subprocess.check_output(command, text=True)
owners = {job:record['campaign'] for job, record in accounting.items()}
owners['1668242'] = 'user interactive allocation; untouched'
mapping = {}
for row in stdout.splitlines():
    identity = row.split('|')[0]; job = identity.split('_')[0]
    assert job in owners, ('unmapped active job', row)
    mapping[identity] = dict(row=row, owner=owners[job])
accounting['active_queue'] = dict(command=command, stdout=stdout, mapping=mapping)
checks = {}
commands = [(name, ['python3', '-m', 'unittest', 'discover', '-s', 'tools/'+name, '-p', 'test_*.py']) for name in ('bench', 'slurm')]
commands += [(name, ['python3', 'tools/docs/'+name+'.py']) for name in ('check-doc-size', 'check-progress', 'check-rule-policy-boundary', 'check-proof-rule-sync', 'check-section-refs', 'check-citations', 'check-option-coverage')]
for name, command in commands:
    r = subprocess.run(command, cwd=root, capture_output=True, text=True, check=True)
    checks[name] = dict(command=command, stdout=r.stdout, stderr=r.stderr)
    print(name, 'PASS', flush=True)
report = root/'report-to-andrew/20260910-0058-vivify-snapshot'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)), sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)}, protected_campaign_hashes=protected,
    source_manifest=dict(path=str((build/'manifest.json').relative_to(root)), sha256=sha(build/'manifest.json')),
    accounting=accounting, evidence=evidence, campaign_states=states, checks=checks,
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex', 'report.pdf')},
    cleanup='Only65799 bytes of generated fixed-proof inputs/certificates removed by owning6827 helper; reproducible from retained fixture. All published checked records clean. Live scratch untouched. Failed fixture logs/source and original profiles remain retained. No fuzz artifact access.',
    limitation='6822 isolated/unfuzzed/NOT adopted;6730 remains current. No speedup, promotion, full50 target closure or memory parity. Scheduling/CDCL unchanged by cost patch, not globally proved on par. Continue exactly three predeclared runtime waves; checks async, invalidity pauses dependents. No Rank21 probes, FlatLists, denied transfer/push retry. Andrew report local/unsent; winddown05:45/cutoff06:00EDT September10.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
for name, records in states.items():
    print(name, len(records), 'published', sum(v.get('state', v).get('checked', False) for v in records.values()), 'checked')
