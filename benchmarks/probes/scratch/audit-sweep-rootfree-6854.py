#!/usr/bin/env python3
"""Freeze one bounded cost checkpoint and audit unchanged campaign authorities."""
from pathlib import Path
import datetime as dt
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-10-sweep-rootfree-audit-6854.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-10-vivify-snapshot-audit-6836.json'
prior = json.loads(prior_path.read_text())
def bind(group):
    for rel,digest in group.items(): assert sha(root/rel) == digest,rel
bind({r:d for r,d in prior['owned_hashes'].items() if r != 'PROGRESS.md'})
bind(prior['protected_campaign_hashes']); bind(prior['andrew_report'])
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-sweep-rootfree-cost.md'}
def own(p): owned.add(str(p.relative_to(root)))
for number in range(6838,6856):
    for p in (root/'benchmarks/probes/scratch').glob(f'*-{number}.*'):
        if p.suffix in ('.py','.cpp'): own(p)
build = root/'benchmarks/runs/20260910-6838-sweep-rootfree-build'
b = json.loads((build/'manifest.json').read_text())
for rel,digest in b['files'].items(): assert sha(root/b['source']/rel) == digest,rel
bind(b['binaries'])
for rel,digest in b['patches'].items(): assert sha(build/rel) == digest; own(build/rel)
for rel,digest in b['logs'].items(): assert sha(build/rel) == digest
evidence = {}
def summary(p):
    r = json.loads(p.read_text()); bind(r.get('hashes',{}))
    own(p); evidence[str(p.relative_to(root))] = sha(p)
    return r
for name in ('manifest.json','parent.json','failed-test_sweep_rootfree.cpp'): own(build/name)
summary(build/'fixture-failure-6840.json'); summary(build/'scope-6853.json')
failure = root/'benchmarks/runs/20260910-6841-sweep-rootfree-control'
summary(failure/'fixture-failure-6842.json'); own(failure/'failed-test_sweep_rootfree.cpp')
control = root/'benchmarks/runs/20260910-6843-sweep-rootfree-control'
r = summary(control/'control-6843.json'); assert r['outputs']['previous'] == r['outputs']['candidate']
for c in (build,failure,control):
    for p in (c/'logs').glob('*.json'): own(p)
proof = root/'benchmarks/runs/20260910-6845-sweep-rootfree-proofs'
r = summary(proof/'summary.json')
assert r['checked'] == 120 and r['removed_bytes'] == 41208
assert not list(proof.glob('*.cnf')) and not list(proof.glob('*.proof'))
assert r['fixture_sha256'] == sha(root/'benchmarks/probes/scratch/test-sweep-rootfree-proof-6845.cpp')
for p in proof.glob('*.json'): own(p)
summary(root/'benchmarks/runs/20260910-6831-vivify-snapshot-runtime/solver-checkpoint-6833.json')
summary(root/'benchmarks/runs/20260910-6831-vivify-snapshot-runtime/timer-cost-6849.json')
summary(root/'benchmarks/runs/20260910-6846-sweep-rootfree-BC-gate/summary-6848.json')
accounting = {}; states = {}
def account(c,p,count):
    launch = json.loads(p.read_text()); own(p)
    assert launch['manifest_sha256'] == sha(c/'manifest.json')
    command = ['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
    stdout = subprocess.check_output(command,text=True)
    rows = stdout.splitlines(); assert len(rows) == count,(c,stdout)
    for row in rows:
        fields = row.split('|')
        assert fields[1] in ('PENDING','RUNNING','COMPLETING','COMPLETED') and fields[2] == '0:0',row
        assert fields[3] >= launch['accounting_since']
    accounting[launch['job_id']] = dict(campaign=str(c.relative_to(root)),command=command,stdout=stdout)
for name,count,required in (
    ('20260910-6828-vivify-snapshot-BC-gate',66,66),
    ('20260910-6815-inprocessing-release-profiles',4,4),
    ('20260910-6831-vivify-snapshot-runtime',18,18),
    ('20260910-6846-sweep-rootfree-BC-gate',66,66),
    ('20260910-6850-sweep-rootfree-runtime',18,0),
):
    c = root/'benchmarks/runs'/name
    m = json.loads((c/'manifest.json').read_text()); own(c/'manifest.json')
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
command = ['squeue','-h','-u','hwu','-o','%i|%j|%T|%P|%C|%m']
stdout = subprocess.check_output(command,text=True)
owners = {j:r['campaign'] for j,r in accounting.items()}; owners['1668242'] = 'user allocation; untouched'
mapping = {}
for row in stdout.splitlines():
    identity = row.split('|')[0]; job = identity.split('_')[0]
    assert job in owners,('unmapped active job',row)
    mapping[identity] = dict(row=row,owner=owners[job])
accounting['active_queue'] = dict(command=command,stdout=stdout,mapping=mapping)
checks = {}
commands = [(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations','check-option-coverage')]
for name,command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name] = dict(command=command,stdout=p.stdout,stderr=p.stderr)
    print(name,'PASS',flush=True)
report = root/'report-to-andrew/20260910-0128-sweep-rootfree'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},protected_campaign_hashes=prior['protected_campaign_hashes'],
    accounting=accounting,campaign_states=states,evidence=evidence,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='41208 bytes of regenerable fixed inputs/proofs removed only by owning6845 checker helper. Failed fixtures/logs and profiles retained; live checker scratch untouched. Published checked outcomes clean. No fuzz artifact access.',
    limitation='6730 remains current;6838 isolated/unfuzzed/NOT adopted,6822 timing neutral/adverse NOT adopted. No global scheduling/CDCL parity, speedup, memory parity or full50 closure. Continue predeclared6850 waves after exact solver records; proofs async. No Rank21, FlatLists, promotion, denied transfer/push retry. Report local/unsent; winddown05:45/cutoff06:00EDT September10.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print(target)
for name,records in states.items(): print(name,len(records),'published',sum(v['state']['checked'] for v in records.values()),'checked')
