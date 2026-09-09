#!/usr/bin/env python3
"""Intentional diagnostic checkpoint: exact launches, source integrity, no fuzz."""
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-rank45-cache-audit-6420.json'; assert not target.exists()
prior=json.loads((root/'benchmarks/reports/2026-09-09-vivify-count-cache-audit-6400.json').read_text())
for rel,digest in prior['protected_campaign_hashes'].items(): assert sha(root/rel)==digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned={'PROGRESS.md','tools/bench/decision_prefix.py','tools/bench/test_decision_prefix.py',
       'benchmarks/reports/2026-09-09-rank45-scheduling-and-cache-gates.md'}
local={}; runs={}; checks={}
def retain(p):
    rel=str(p.relative_to(root))
    if p.stat().st_size>=1<<20 or p.suffix in ('.log','.out','.o') or '/traces/' in rel:
        local[rel]=dict(bytes=p.stat().st_size,sha256=sha(p))
    else: owned.add(rel)
for n in range(6402,6422):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.py'): retain(p)
for name in ('6357-compact-stamp','6383-vivify-count-cache','6413-rank45-schedule'):
    c=root/f'benchmarks/runs/20260909-{name}-build'; b=json.loads((c/'manifest.json').read_text())
    for rel,digest in b['files'].items(): assert sha(root/b['source']/rel)==digest,rel
    for rel,digest in b['binaries'].items(): assert sha(root/rel)==digest,rel
    if name.startswith('6413'):
        assert b['release_exact_parent'] and sha(c/'candidate.patch')==b['patch_sha256']
        for kind,count in (('release',106),('trace',105)):
            assert f'100% tests passed out of {count}' in (c/'logs'/f'{kind}-ctest.log').read_text()
        for p in c.iterdir():
            if p.suffix in ('.json','.patch','.tsv'): retain(p)
        for p in (c/'logs').iterdir(): retain(p)
names=('6320-substitute-collapse-ranks1-50','6362-compact-stamp-BC-gate','6366-compact-stamp-ranks1-50',
       '6372-rank32-release-profile','6374-compact-stamp-three-wave-runtime','6387-vivify-count-cache-BC-gate',
       '6389-rank35-stamp-extension','6402-rank45-stamp-prefix','6405-count-cache-three-wave-runtime',
       '6407-count-cache-ranks1-50','6410-rank45-transitive-census','6415-rank45-transitive-schedule')
for name in names:
    c=root/('benchmarks/runs/20260909-'+name); m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',m.get('hashes',{})).items(): assert sha(c/rel)==digest,rel
    launches=sorted(c.glob('wave*-submission.json')) if 'three-wave' in name else [c/'launch.json']
    states={}; accounting={}; latest={}; diagnostics=[]
    for p in launches:
        launch=json.loads(p.read_text()); assert sha(c/'manifest.json')==launch['manifest_sha256']
        command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
        output=subprocess.check_output(command,text=True)
        accounting[p.name]=dict(command=command,stdout=output,launch=launch)
        for line in output.splitlines():
            fields=line.split('|'); task=fields[0].split('_')
            if len(task)==2 and task[1].isdigit(): states[int(task[1])]=fields[1]
        retain(p)
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text())
            if 'task' in r:
                status=classify(r,folder=='results')
                assert status['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,status)
                if folder=='results': assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
                latest[r['task']]=(r,folder=='results')
                if 'three-wave' in name:
                    b=m['work_oracles'][r['arm']][str(r['rank'])]; assert sha(root/b['path'])==b['sha256']
                    oracle=json.loads((root/b['path']).read_text())
                    for key in ('certificate_sha256','certificate_bytes','verdict','instance'): assert r[key]==oracle[key]
                    nt=lambda v:[s for s in v['solver_statistics'] if not s.startswith('c timing')]
                    assert nt(r)==nt(oracle)
            else:
                assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists(),p
                assert r['binary_sha256']==sha(c/'pins'/r['arm'])
                for filename,digest in r['trace_sha256'].items():
                    q=c/'traces'/filename; assert sha(q)==digest; retain(q)
                diagnostics.append(r['arm'])
            retain(p)
    latest={t:classify(r,final,states.get(t)) for t,(r,final) in latest.items()}
    assert all(v['status'] in ('pending_validation','checked_SAT','checked_UNSAT') for v in latest.values()),(name,latest)
    assert all(s in ('PENDING','RUNNING','COMPLETED','COMPLETING') for s in states.values()),(name,states)
    for p in c.rglob('*'):
        if p.is_file() and (p.suffix in ('.py','.sbatch','.patch') or p.name in ('manifest.json','matrix.tsv','summary.json') or p.name.startswith('solver-checkpoint-')): retain(p)
    runs[name]=dict(accounting=accounting,outcomes=latest,completed_diagnostic_arms=diagnostics)
    print(name,len(latest),'solver records',sum(v['checked'] for v in latest.values()),'checked',diagnostics,flush=True)
for p in (root/'benchmarks/reports/2026-09-09-count-cache-full50-6418').glob('*'): retain(p)
commands=[(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    p=subprocess.run(command,cwd=root,text=True,capture_output=True,check=True)
    checks[name]=dict(command=command,stdout=p.stdout,stderr=p.stderr); print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-0745-rank45-cache'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={r:sha(root/r) for r in sorted(owned)},local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'],runs=runs,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='No manual deletion or live checker scratch inspection. Completed bounded runners hash/delete their own partial proofs and decompressed inputs; final-proof acceptance is separate. All frozen pins, references andcpu-q5784 intact.',
    failure_classification='6404 failed a derived start-counter assumption, not a solver/proof oracle. Frozen failed validator preserved;6409 uses actual observer accounting with two maintained regression tests. Early incomplete-wave submission refused before writing or submitting, then wave1 submitted once when complete. Initial apply_patch context mismatch made no edits. No solver/proof/model/build failure or excluded measurement.',
    next='Continue exact6405 timing and6407 full50 outcomes without waiting for proof checks; validate6415 schedule replay before comparing keys. Preserve Rank32 closure and use general source-backed repairs only. No source promotion, fuzz inspection, denied transfer or push retry.'),indent=2)+'\n')
print(target)
