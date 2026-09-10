#!/usr/bin/env python3
"""Freeze the approved embedded-cost checkpoint, preserving all earlier runs."""
from pathlib import Path
import datetime as dt
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-10-subsolver-cost-audit-6885.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-10-sweep-rootfree-audit-6854.json'
prior = json.loads(prior_path.read_text())
def bind(group):
    for rel,digest in group.items(): assert sha(root/rel) == digest,rel
bind({r:d for r,d in prior['owned_hashes'].items() if r != 'PROGRESS.md'})
bind(prior['protected_campaign_hashes']); bind(prior['andrew_report'])
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned = {'PROGRESS.md','tools/slurm/wait.py','tools/slurm/test_wait.py',
    'benchmarks/reports/2026-09-10-subsolver-cost.md',
    'benchmarks/reports/2026-09-10-subsolver-binary-design.md'}
def own(p): owned.add(str(p.relative_to(root)))
for number in range(6856,6887):
    for p in (root/'benchmarks/probes/scratch').glob(f'*-{number}.*'):
        if p.suffix in ('.py','.cpp'): own(p)
evidence = {}
def summary(p):
    r = json.loads(p.read_text()); bind(r.get('hashes',{}))
    own(p); evidence[str(p.relative_to(root))] = sha(p)
    return r
for stem,controls,scope in (
    ('20260910-6857-subsolver-shrink-build',(6858,),6867),
    ('20260910-6869-subsolver-binary-build',(6870,6874,6881),6875),
):
    c = root/'benchmarks/runs'/stem
    b = summary(c/'manifest.json')
    for rel,digest in b['files'].items(): assert sha(root/b['source']/rel) == digest,rel
    bind(b['binaries'])
    for rel,digest in b['patches'].items(): assert sha(c/rel) == digest; own(c/rel)
    for rel,digest in b['logs'].items(): assert sha(c/rel) == digest
    own(c/'parent.json')
    for number in controls:
        r = summary(c/f'control-{number}.json')
        assert r['outputs']['previous'] == r['outputs']['candidate']
    summary(c/f'scope-{scope}.json')
    for p in (c/'logs').glob('*.json'): own(p)
removed = 0
for name in ('20260910-6860-subsolver-shrink-proofs','20260910-6872-subsolver-binary-proofs'):
    c = root/'benchmarks/runs'/name
    r = summary(c/'summary.json')
    assert r['checked'] == 120 and r['removed_bytes'] == 41208
    assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
    removed += r['removed_bytes']
    for p in c.glob('*.json'): own(p)
a = summary(root/'benchmarks/runs/20260910-6856-subsolver-propagation-annotation/summary.json')
assert a['temporary_expansions_deleted_bytes'] == 724221844
assert not list((root/'benchmarks/runs/20260910-6856-subsolver-propagation-annotation').glob('annotation-*'))
for rel in (
    '20260910-6850-sweep-rootfree-runtime/solver-checkpoint-6852.json',
    '20260910-6850-sweep-rootfree-runtime/timer-cost-6868.json',
    '20260910-6861-subsolver-shrink-BC-gate/summary-6863.json',
): summary(root/'benchmarks/runs'/rel)
accounting = {}; states = {}
def account(c,p,count):
    launch = json.loads(p.read_text()); own(p)
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
    stdout = subprocess.check_output(command,text=True)
    rows = [line.split('|') for line in stdout.splitlines()]
    assert len(rows) == count,(c,stdout)
    assert len({r[0] for r in rows}) == count,('ambiguous accounting',stdout)
    assert len({r[3] for r in rows}) == 1,('recycled accounting',stdout)
    for fields in rows:
        assert fields[0].startswith(launch['job_id']+'_'),fields
        assert fields[1] in ('PENDING','RUNNING','COMPLETING','COMPLETED') and fields[2] == '0:0',fields
        assert fields[3] >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)),command=command,stdout=stdout)
for name,count,required in (
    ('20260910-6828-vivify-snapshot-BC-gate',66,66),
    ('20260910-6815-inprocessing-release-profiles',4,4),
    ('20260910-6831-vivify-snapshot-runtime',18,18),
    ('20260910-6846-sweep-rootfree-BC-gate',66,66),
    ('20260910-6850-sweep-rootfree-runtime',18,18),
    ('20260910-6861-subsolver-shrink-BC-gate',66,66),
    ('20260910-6864-subsolver-shrink-runtime',18,6),
    ('20260910-6873-subsolver-binary-BC-gate',66,0),
    ('20260910-6882-subsolver-binary-runtime',18,0),
):
    c = root/'benchmarks/runs'/name
    if not c.exists():
        assert name == '20260910-6882-subsolver-binary-runtime'
        continue
    m = summary(c/'manifest.json')
    for rel,digest in m['pin_sha256'].items():
        assert sha(c/rel) == digest,rel
        if (c/rel).suffix in ('.py','.patch','.tsv','.sbatch'): own(c/rel)
    if isinstance(m.get('dependencies'),dict): bind(m['dependencies'])
    if c.name.endswith('-runtime'):
        for p in sorted(c.glob('wave*-submission.json')): account(c,p,6)
    else: account(c,c/'launch.json',count)
    current = {}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r = json.loads(p.read_text()); state = classify(r,folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            assert r['task'] == int(p.stem) and 0 <= r['task'] < count
            if folder == 'results': assert r['binary_sha256'] == m['arms'][r['arm']]['sha256']
            if 'work_oracles' in m:
                binding = m['work_oracles'][r['arm']][str(r['rank'])]
                assert sha(root/binding['path']) == binding['sha256']
                old = json.loads((root/binding['path']).read_text())
                for key in ('certificate_sha256','certificate_bytes','verdict','instance'): assert r[key] == old[key],(p,key)
                nt = lambda v:[s for s in v['solver_statistics'] if not s.startswith('c timing')]
                assert nt(r) == nt(old),p
            own(p); current[r['task']] = dict(rank=r['rank'],arm=r['arm'],state=state)
    assert len(current) >= required
    states[str(c.relative_to(root))] = current
    for p in c.glob('summary-*.json'): summary(p)
command = ['squeue','-h','-u','hwu','-o','%i|%j|%T|%P|%C|%m']
stdout = subprocess.check_output(command,text=True)
owners = {j:r['campaign'] for j,r in accounting.items()}; owners['1668242'] = 'user allocation; untouched'
mapping = {}
for row in stdout.splitlines():
    identity = row.split('|')[0]; job = identity.split('_')[0]
    assert job in owners,('unmapped active job',row)
    mapping[identity] = dict(row=row,owner=owners[job])
accounting['active_queue'] = dict(command=command,stdout=stdout,mapping=mapping)
monitor = subprocess.check_output(['ps','-p','271019','-o','pid=,comm=,args='],text=True)
assert 'session_deadline.py' in monitor and '2026-09-10T06:00:00-04:00' in monitor
checks = {}
commands = [(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations','check-option-coverage')]
for name,command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name] = dict(command=command,stdout=p.stdout,stderr=p.stderr)
    print(name,'PASS',flush=True)
report = root/'report-to-andrew/20260910-0208-subsolver-cost'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},protected_campaign_hashes=prior['protected_campaign_hashes'],
    accounting=accounting,campaign_states=states,evidence=evidence,checks=checks,deadline_monitor=monitor,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup=dict(fixed_regenerable_input_proof_bytes=removed,temporary_profile_expansion_bytes=a['temporary_expansions_deleted_bytes'],
        note='Only owning fixed-check/annotation helpers removed exact temporary products. Compressed originals, failed controls, all live scratch and prior authorities retained. Checked published outcomes clean. No fuzz artifact access.'),
    limitation='6730 current/unpromoted/unfuzzed, full50 target unmet. User approved6869 prototype only;6869/6857 experimental,6838/6822 unadopted. No global scheduling/CDCL or memory parity. Continue exact solver gates with proofs async; any invalidity pauses dependents. No Rank21, FlatLists, promotion, denied transfer/push retry. Report local/unsent. Winddown05:45/cutoff06:00EDT September10.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(target)
for name,records in states.items(): print(name,len(records),'published',sum(v['state']['checked'] for v in records.values()),'checked')
