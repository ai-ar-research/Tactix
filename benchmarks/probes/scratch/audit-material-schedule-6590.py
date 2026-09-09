#!/usr/bin/env python3
"""Own the approved cutover, failures, independent gates and Rank50 diagnostic."""
import datetime, json, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-material-schedule-audit-6590.json'
assert not target.exists()
prior_path=root/'benchmarks/reports/2026-09-09-material-state-audit-6577.json'
prior=json.loads(prior_path.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in prior[group].items():
        if rel!='PROGRESS.md': assert sha(root/rel)==digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned={'PROGRESS.md','docs/calculus.md','docs/implementation-notes.md','docs/scheduling-parity-ledger.md',
       'benchmarks/reports/2026-09-09-material-scheduling-cutover.md'}
scripts=('prepare-material-schedule-6579.py','build-material-schedule-6580.py',
 'test-material-proofs-6581.cpp','check-material-proofs-6582.py',
 'material-schedule-initial-fixture-6583.cpp','material-schedule-fixture-failure-6583.md',
 'material-proof-initial-fixture-6583.cpp','material-proof-diagnostic-6583.cpp','diagnose-material-proof-6583.py',
 'prepare-material-schedule-BC-6584.py','submit-material-schedule-BC-6585.py','validate-material-schedule-BC-6586.py',
 'prepare-material-schedule-rank50-6587.py','submit-material-schedule-rank50-6588.py',
 'validate-material-schedule-rank50-6589.py','audit-material-schedule-6590.py','land-material-schedule-6591.py',
 'prepare-material-runtime-6592.py','submit-material-runtime-6593.py')
owned.update('benchmarks/probes/scratch/'+p for p in scripts)
local={}
def bind(p,expected=None,own=False):
    digest=sha(p)
    if expected is not None: assert digest==expected,p
    rel=str(p.relative_to(root)); local[rel]=digest
    if own: owned.add(rel)
    return digest
def read(p,own=False):
    bind(p,own=own); return json.loads(p.read_text())
build=root/'benchmarks/runs/20260909-6579-material-schedule-build'
b=read(build/'manifest.json',True); approval=read(build/'parent.json',True)
assert approval['approval'].startswith('User replied Yes')
bind(root/approval['path'],approval['sha256'])
bind(root/approval['design']['path'],approval['design']['sha256'])
for rel,digest in b['files'].items(): bind(root/b['source']/rel,digest)
for rel,digest in b['binaries'].items(): bind(root/rel,digest)
for rel,digest in {**b['patches'],**b['logs']}.items(): bind(build/rel,digest,True)
assert b['binaries']['build/pin-6579-material-schedule-release/tactix']==b['binaries']['build/pin-6579-material-schedule-bc/tactix']
for kind,count in (('release',110),('bc',110),('trace',109)):
    assert f'100% tests passed out of {count}' in (build/'logs'/f'{kind}-ctest-2.log').read_text()
    assert 'material_schedule' in (build/'logs'/f'{kind}-ctest.log').read_text()
proof=root/'benchmarks/runs/20260909-6582-material-proof-control'
p=read(proof/'summary.json',True)
assert p['cases']==24 and p['final_proofs_checked']==12
for rel,digest in p['hashes'].items(): bind(root/rel,digest)
for r in p['records']:
    assert r['fixture']['exit_code']==0 and r['effects']['strengthened']>0
    if r['missing']: assert r['effects']['models']>0
    else:
        check=r['checker']; assert check['exit_code']==0
        assert ('c VERIFIED' if r['mode'] else 's VERIFIED') in check['stdout']
        bind(Path(check['command'][0]),check['sha256'])
assert not list(proof.glob('*.cnf')) and not list(proof.glob('*.proof'))
for path in proof.iterdir(): bind(path,own=path.suffix in ('.json','.log'))

accounting=dict(prior['accounting']); gates={}
for name in ('20260909-6574-material-state-BC-gate','20260909-6584-material-schedule-BC-gate'):
    c=root/'benchmarks/runs'/name; m=read(c/'manifest.json',True); launch=read(c/'launch.json',True)
    assert launch['manifest_sha256']==sha(c/'manifest.json')
    for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest,rel.endswith(('.py','.tsv','.sbatch','.patch')))
    checked=0
    for path in sorted((c/'results').glob('*.json')):
        r=read(path,True); status=classify(r,True)
        assert status['checked'] and status['clean'],(path,status)
        assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
        checked+=1
    for path in sorted((c/'provisional').glob('*.json')):
        r=read(path,True); assert classify(r,False)['status']=='pending_validation'
    gates[name]=dict(checked_clean=checked,validation_pending=checked!=66)
    command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
    output=subprocess.check_output(command,text=True)
    assert len(output.splitlines())==66
    for line in output.splitlines():
        f=line.split('|'); assert f[1] in ('COMPLETED','RUNNING','COMPLETING') and f[2]=='0:0',line
    accounting[name]=dict(command=command,stdout=output,launch=launch)
summary=read(root/'benchmarks/runs/20260909-6584-material-schedule-BC-gate/summary-6586.json',True)
assert summary['solver_records']==66 and len(summary['pairs'])==33
for rel,digest in summary['hashes'].items(): bind(root/rel,digest)
rank=root/'benchmarks/runs/20260909-6587-rank50-material-schedule'
r=read(rank/'summary-6589.json',True); assert r['observer_neutral'] and r['scratch_deleted']
for rel,digest in r['hashes'].items(): bind(root/rel,digest)
m=read(rank/'manifest.json',True); launch=read(rank/'launch.json',True)
for rel,digest in launch['dependencies'].items(): bind(root/rel,digest)
for rel,digest in m['hashes'].items(): bind(rank/rel,digest,rel.endswith(('.py','.sbatch')))
for path in sorted((rank/'results').glob('*.json')): read(path,True)
for path in sorted((rank/'logs').iterdir()): bind(path,own=True)
command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
output=subprocess.check_output(command,text=True)
assert len(output.splitlines())==2
for line in output.splitlines():
    f=line.split('|'); assert f[1]=='COMPLETED' and f[2]=='0:0',line
accounting[rank.name]=dict(command=command,stdout=output,launch=launch)
runtime=root/'benchmarks/runs/20260909-6592-rank50-material-three-wave-runtime'
rm=read(runtime/'manifest.json',True); rl=read(runtime/'wave0-submission.json',True)
assert rl['tasks']==[0,1] and rl['manifest_sha256']==sha(runtime/'manifest.json')
for rel,digest in rm['pin_sha256'].items(): bind(runtime/rel,digest,rel.endswith(('.py','.tsv','.sbatch','.patch')))
for rel,digest in rm['dependencies'].items(): bind(root/rel,digest)
runtime_records={}
for folder in ('provisional','results'):
    for path in sorted((runtime/folder).glob('*.json')):
        rr=read(path,True); status=classify(rr,folder=='results')
        assert status['status'] in ('pending_validation','checked_SAT'),(path,status)
        assert rr['task'] in (0,1) and rr['rank']==50
        runtime_records[rr['task']]=dict(status=status,seconds=rr['solve']['wall_seconds'])
command=['sacct','-n','-X','--array','-j',rl['job_id'],'-S',rl['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
output=subprocess.check_output(command,text=True)
assert len(output.splitlines())==2
for line in output.splitlines():
    f=line.split('|'); assert f[1] in ('COMPLETED','RUNNING','COMPLETING','PENDING') and f[2]=='0:0',line
accounting[runtime.name]=dict(command=command,stdout=output,launch=rl)
checks={}
commands=[(name,['python3','-m','unittest','discover','-s','tools/'+name,'-p','test_*.py']) for name in ('bench','slurm')]
commands += [(name,['python3','tools/docs/'+name+'.py']) for name in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    result=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name]=dict(command=command,stdout=result.stdout,stderr=result.stderr)
    print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-1610-material-scheduling-cutover'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
out=dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'],accounting=accounting,
    checks=checks,gates=gates,runtime_wave0=runtime_records,full50=prior['full50'],approval=approval['approval'],
    rank50=dict(decision_difference=r['decision_difference'],parent_decision_difference=r['parent_decision_difference'],boundary_window=r['boundary_window']),
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    cleanup='Only exact fixed-fixture CNF/proof outputs removed after checks; failed-output hashes retained. Background checker scratch untouched; completed run records require zero residue.',
    limitations='Isolated approved cutover, no promotion or timing claim. Initial fixed-test failures and first legacy micro-proof hash omission disclosed. Source fuzz unavailable/Claude-owned; FlatLists and push not authorized. Full50/memory convergence remains unmet.')
with target.open('x') as stream: json.dump(out,stream,indent=2)
print(target)
