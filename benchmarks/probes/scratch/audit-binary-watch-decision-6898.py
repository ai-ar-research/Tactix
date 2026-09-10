#!/usr/bin/env python3
"""Freeze the binary rejection and explicitly classified object inspection."""
from pathlib import Path
import datetime as dt
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root/'benchmarks/reports/2026-09-10-binary-watch-audit-6898.json'
assert not target.exists(), 'successful writer frozen'
prior_path = root/'benchmarks/reports/2026-09-10-subsolver-timing-audit-6891.json'
prior = json.loads(prior_path.read_text())
def bind(group):
    for rel, digest in group.items():
        assert sha(root/rel) == digest, rel
bind({r:d for r,d in prior['owned_hashes'].items() if r != 'PROGRESS.md'})
bind(prior['protected_campaign_hashes'])
bind(prior['andrew_report'])
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'], cwd=root)
owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-binary-watch-decision.md'}
for number in range(6893,6900):
    for p in (root/'benchmarks/probes/scratch').glob(f'*-{number}.py'):
        owned.add(str(p.relative_to(root)))
evidence = {}
def own(p):
    owned.add(str(p.relative_to(root)))
def summary(p):
    r = json.loads(p.read_text())
    bind(r.get('hashes', {}))
    own(p)
    evidence[str(p.relative_to(root))] = sha(p)
    return r

c = root/'benchmarks/runs/20260910-6893-watch-dispatch-build'
compile_record = summary(c/'compile-6894.json')
shape = summary(c/'code-shape-6895.json')
recovery = summary(c/'inspection-6896.json')
assert shape['differing_body_bytes'] == 0
assert shape['body_bytes'] == {'previous':3865,'candidate':3865}
assert all(r['byte_identical'] for r in shape['regions'].values())
object_bindings = {}
for arm, r in recovery['records'].items():
    assert r['original_expected_sha256'] == compile_record['objects'][arm]['sha256']
    assert r['recovered_original']['sha256'] == r['original_expected_sha256']
    assert r['rewritten_container']['path'] == compile_record['objects'][arm]['path']
    assert r['rewritten_container']['sha256'] != r['original_expected_sha256']
    for key in ('rewritten_container','recovered_original'):
        p, digest = r[key]['path'], r[key]['sha256']
        assert sha(root/p) == digest, p
        object_bindings[p] = digest
    assert r['body_bytes'] == 3865
assert len({r['body_sha256'] for r in recovery['records'].values()}) == 1
assert shape['temporary_section_bytes_removed'] + recovery['temporary_bytes_removed'] == 863596
for p in (c/'logs').glob('*.json'):
    own(p)
own(c/'parent.json')
own(c/'compile-scope.patch')
binary = root/'benchmarks/runs/20260910-6882-subsolver-binary-runtime'
checkpoint = summary(binary/'solver-checkpoint-6884.json')
assert checkpoint['solver_outcomes'] == 18 and checkpoint['exact_per_arm_work_and_proof']
cost = summary(binary/'timer-cost-6887.json')
assert cost['solved'] == dict(candidate=9,previous=9,SAT=0,UNSAT=18,timeouts=0,solve_losses=0)
for rank in ('32','39'):
    assert cost['ranks'][rank]['paired']['wall_seconds']['min'] > 1

accounting = {}
states = {}
def account(c, p, count):
    launch = json.loads(p.read_text())
    own(p)
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],
               '-o','JobID,State%40,ExitCode,Submit','-P']
    stdout = subprocess.check_output(command, text=True)
    rows = [line.split('|') for line in stdout.splitlines()]
    assert len(rows) == count and len({r[0] for r in rows}) == count, stdout
    assert len({r[3] for r in rows}) == 1, ('recycled accounting',stdout)
    for r in rows:
        assert r[0].startswith(launch['job_id']+'_')
        assert r[1] in ('PENDING','RUNNING','COMPLETING','COMPLETED') and r[2] == '0:0', r
        assert r[3] >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)),command=command,stdout=stdout)

for rel, previous_states in prior['campaign_states'].items():
    c = root/rel
    count = 18 if c.name.endswith('-runtime') else (4 if c.name.endswith('-profiles') else 66)
    m = summary(c/'manifest.json')
    for pin, digest in m['pin_sha256'].items():
        assert sha(c/pin) == digest, pin
    if isinstance(m.get('dependencies'),dict):
        bind(m['dependencies'])
    if c.name.endswith('-runtime'):
        for p in sorted(c.glob('wave*-submission.json')):
            account(c,p,6)
    else:
        account(c,c/'launch.json',count)
    current = {}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r = json.loads(p.read_text())
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'), (p,state)
            assert r['task'] == int(p.stem) and 0 <= r['task'] < count
            if folder == 'results':
                assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            if 'work_oracles' in m:
                binding = m['work_oracles'][r['arm']][str(r['rank'])]
                assert sha(root/binding['path']) == binding['sha256']
                old = json.loads((root/binding['path']).read_text())
                for key in ('certificate_sha256','certificate_bytes','verdict','instance'):
                    assert r[key] == old[key], (p,key)
                nt = lambda v: [s for s in v['solver_statistics'] if not s.startswith('c timing')]
                assert nt(r) == nt(old), p
            own(p)
            current[r['task']] = dict(rank=r['rank'],arm=r['arm'],state=state)
    assert len(current) == count, (rel,len(current),count)
    for task, old in previous_states.items():
        if old['state']['checked']:
            assert current[int(task)]['state']['checked'], (rel,task)
    states[rel] = current
assert '1679208' in accounting
profiles = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles'
counter_sources = {}
for p in sorted((profiles/'results').glob('*.json')):
    r = json.loads(p.read_text())
    assert r['runtime_context']['perf_statistics'] == []
    counter_sources[str(p.relative_to(root))] = sha(p)
assert len(counter_sources) == 4
p = profiles/'pins/run.py'
assert "command[:2] == ['perf', 'stat']" in p.read_text()
counter_sources[str(p.relative_to(root))] = sha(p)

command = ['squeue','-h','-u','hwu','-o','%i|%j|%T|%P|%C|%m']
stdout = subprocess.check_output(command,text=True)
owners = {j:r['campaign'] for j,r in accounting.items()}
owners['1668242'] = 'user allocation; untouched'
mapping = {}
for row in stdout.splitlines():
    identity = row.split('|')[0]
    job = identity.split('_')[0]
    assert job in owners, ('unmapped active job',row)
    mapping[identity] = dict(row=row,owner=owners[job])
accounting['active_queue'] = dict(command=command,stdout=stdout,mapping=mapping)
monitor = subprocess.check_output(['ps','-p','271019','-o','pid=,comm=,args='],text=True)
assert 'session_deadline.py' in monitor and '2026-09-10T06:00:00-04:00' in monitor
checks = {}
commands = [(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations','check-option-coverage')]
for name, command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name] = dict(command=command,stdout=p.stdout,stderr=p.stderr)
    print(name,'PASS',flush=True)
report = root/'report-to-andrew/20260910-0250-binary-watch-decision'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'],
    accounting=accounting,campaign_states=states,evidence=evidence,checks=checks,deadline_monitor=monitor,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    classified_object_bindings=object_bindings,inspection_classification=recovery['classification'],
    missing_solver_counters=dict(hashes=counter_sources,reason='Frozen profile wrapper replaces perf stat with perf record; empty solver lists. Probe true cycles are not solver counters. No IPC claim or solver failure.'),
    cleanup=dict(temporary_section_output_bytes=863596,fixed_proof_bytes=0,profile_expansion_bytes=0,
                 note='Only6895/6896 exact temporary sections/output objects removed. Retain rewritten containers, recovered originals, source, failures, pins, compressed profiles and all live scratch. No fuzz artifact access.'),
    limitation='6869 rejected after all18 exact solver results:32/39 adverse allpairs,32 RSS adverse.6893 compile-only hotbody identical; no solver run/adoption.6730 remains unpromoted/unfuzzed; production unchanged; full50/global scheduling-CDCL/memory target unmet. Proofs async. Next existing-profile large-watch rebuild localization, not representation approval. No Rank21/FlatLists/promotion/denied push or transfer retry. Report local unsent. Winddown05:45/cutoff06:00EDT September10.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2)
    stream.write('\n')
print(target)
for name, records in states.items():
    print(name,len(records),'published',sum(v['state']['checked'] for v in records.values()),'checked')
