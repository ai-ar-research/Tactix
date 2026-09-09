#!/usr/bin/env python3
"""Hash-bound source, checker, queued-job and documentation checkpoint; no fuzz."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-vivify-count-cache-audit-6400.json'
assert not target.exists()
prior=json.loads((root/'benchmarks/reports/2026-09-09-stamp-full50-profile-audit-6380.json').read_text())
for rel,digest in prior['protected_campaign_hashes'].items(): assert sha(root/rel)==digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned={'PROGRESS.md','benchmarks/reports/2026-09-09-vivify-count-cache-and-adverse-control.md'}
local={}; checks={}; runs={}
def retain(p):
    rel=str(p.relative_to(root))
    if p.stat().st_size>=1<<20 or p.suffix in ('.o','.class','.log','.out') or '/traces/' in rel:
        local[rel]=dict(bytes=p.stat().st_size,sha256=sha(p))
    else: owned.add(rel)
for n in range(6382,6402):
    for extension in ('py','cpp'):
        for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.'+extension): retain(p)
for stem in ('6357-compact-stamp','6383-vivify-count-cache'):
    c=root/f'benchmarks/runs/20260909-{stem}-build'
    b=json.loads((c/'manifest.json').read_text())
    for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest,rel
    for rel,digest in b['binaries'].items(): assert sha(root/rel)==digest,rel
    if stem.startswith('6383'):
        for rel,digest in b['patches'].items(): assert sha(c/rel)==digest,rel
        for p in c.iterdir():
            if p.is_file() and p.suffix in ('.json','.patch','.class'): retain(p)
        for p in (c/'logs').iterdir(): retain(p)
        control=json.loads((c/'control-6384.json').read_text())
        assert control['outputs']['previous']==control['outputs']['candidate']
        assert all(v['size']==40 and v['alignment']==8 for v in control['layouts'].values())
        for group in ('logs','fixtures','parent_objects'):
            for rel,digest in control[group].items(): assert sha(root/rel)==digest,rel
        for kind,count in (('release',107),('bc',107),('trace',106)):
            assert f'100% tests passed out of {count}' in (c/'logs'/f'{kind}-ctest.log').read_text()
c=root/'benchmarks/runs/20260909-6386-vivify-count-cache-proofs'
s=json.loads((c/'summary.json').read_text()); assert s['checked']==48
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
for r in s['records']:
    assert sha(Path(r['command'][0]))==r['checker_sha256']
    assert 'VERIFIED' in r['stdout'] and r['proof_bytes']>0
for p in c.iterdir():
    if p.suffix in ('.json','.log'): retain(p)
names=('20260909-6320-substitute-collapse-ranks1-50',
       '20260909-6362-compact-stamp-BC-gate','20260909-6366-compact-stamp-ranks1-50',
       '20260909-6372-rank32-release-profile','20260909-6374-compact-stamp-three-wave-runtime',
       '20260909-6387-vivify-count-cache-BC-gate','20260909-6389-rank35-stamp-extension')
for name in names:
    c=root/'benchmarks/runs'/name; m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',m.get('hashes',{})).items(): assert sha(c/rel)==digest,rel
    launches=sorted(c.glob('wave*-submission.json')) if '6374-' in name else [c/'launch.json']
    states={}; accounting={}; latest={}
    for p in launches:
        launch=json.loads(p.read_text()); assert sha(c/'manifest.json')==launch['manifest_sha256']
        command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
        output=subprocess.check_output(command,text=True)
        accounting[p.name]=dict(launch=launch,command=command,stdout=output)
        for line in output.splitlines():
            fields=line.split('|'); task=fields[0].split('_')
            if len(task)==2 and task[1].isdigit(): states[int(task[1])]=fields[1]
        retain(p)
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text())
            if 'task' in r:
                # Historical provisional records remain after final publish.
                # Merge first; terminal accounting applies only to the winner.
                state=classify(r,folder=='results')
                assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
                if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
                latest[r['task']]=(r,folder=='results')
                if '6374-' in name:
                    binding=m['work_oracles'][r['arm']][str(r['rank'])]
                    assert sha(root/binding['path'])==binding['sha256']
                    oracle=json.loads((root/binding['path']).read_text())
                    for key in ('certificate_sha256','certificate_bytes','verdict','instance'): assert r[key]==oracle[key],(p,key)
                    nt=lambda v:[s for s in v['solver_statistics'] if not s.startswith('c timing')]
                    assert nt(r)==nt(oracle),p
            else:
                assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists(),p
                for filename,digest in r['trace_sha256'].items():
                    q=c/'traces'/filename; assert sha(q)==digest; retain(q)
            retain(p)
    latest={task:classify(r,final,states.get(task)) for task,(r,final) in latest.items()}
    assert all(v['status'] in ('pending_validation','checked_SAT','checked_UNSAT') for v in latest.values()),(name,latest)
    expected=18 if '6374-' in name else 66 if 'BC-gate' in name else 2 if any(s in name for s in ('6372-','6389-')) else 50
    if '6387-' not in name and '6389-' not in name: assert len(latest)==expected,(name,len(latest))
    # A queued task without a solver record is unfinished, not a successful gate.
    assert all(state in ('PENDING','RUNNING','COMPLETED','COMPLETING') for state in states.values()),(name,states)
    for p in c.rglob('*'):
        if p.is_file() and (p.suffix in ('.py','.sbatch','.patch') or p.name in ('manifest.json','matrix.tsv','summary.json','solver-checkpoint-6382.json','solver-checkpoint-6392.json')): retain(p)
    runs[name]=dict(accounting=accounting,outcomes=latest,required_solver_records=expected)
    print(name,len(latest),'solver records',sum(v['checked'] for v in latest.values()),'checked',flush=True)
timing=root/'benchmarks/runs/20260909-6374-compact-stamp-three-wave-runtime/solver-checkpoint-6382.json'
s=json.loads(timing.read_text()); assert s['solver_outcomes']==18 and s['exact_per_arm_work_and_proof']
assert all(v['absolute_range_within_two_percent'] for v in s['ranks'].values())
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
commands=[(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    p=subprocess.run(command,cwd=root,text=True,capture_output=True,check=True)
    checks[name]=dict(command=command,stdout=p.stdout,stderr=p.stderr); print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-0710-vivify-count-cache'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={r:sha(root/r) for r in sorted(owned)},local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'],runs=runs,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='No manual deletion or live scratch inspection.6386 removed65799 bytes of48 generated checked proof/input pairs; none remain. Queued6389 owns future partial-proof cleanup. Frozen production/references/cpu-q unchanged.',
    failure_classification='No solver/build/proof/model failure.6382 refused an incomplete solver wave before creating a snapshot, then succeeded when18 records existed; no measurement excluded. Audit6400 initially applied terminal accounting to a historical provisional record before merging its existing checked final; corrected merge order before output creation. Existing terminal-missing/final-status classifier tests cover both classes; no parser change or weakened check. Read-only filename-discovery misses were harmless. Initial Andrew PDF had2.72pt reference-line overflow: log retained, local pre-delivery font size corrected and two clean compiles required.',
    next='Check exact jobs1677717/1677718, do not resubmit.6392 requires66expected plus33candidate parent-work/proof identities; then fullfixed50 and3paired32/33/35 cache-cost waves.6391 validates inherited100K closure and complete1M suffix. Checkers asynchronous; sourcefuzz/memory/trackerapproval/promotion remain open. No denied push/transfer retry or fuzz inspection.'),indent=2)+'\n')
print(target)
