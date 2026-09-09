#!/usr/bin/env python3
"""Audit current evidence and live dependencies without touching checker scratch."""
import json
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target=root/'benchmarks/reports/2026-09-09-replicas-census-audit-6347.json'
assert not target.exists()
prior=json.loads((root/'benchmarks/reports/2026-09-09-collapse-clean-audit-6337.json').read_text())
for rel,digest in prior['protected_campaign_hashes'].items():assert sha(root/rel)==digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
b=json.loads((root/'benchmarks/runs/20260909-6305-substitute-collapse-build/manifest.json').read_text())
for rel,digest in b['files'].items():assert sha(root/b['source']/rel)==digest,rel
for rel,digest in b['binaries'].items():assert sha(root/rel)==digest,rel
owned={'PROGRESS.md','tools/bench/transitive_trace.py','tools/bench/test_transitive_trace.py',
       'benchmarks/reports/2026-09-09-collapse-replicas-and-transitive-census.md'}
local={};runs={};checks={}
def retain(p):
    rel=str(p.relative_to(root))
    if p.stat().st_size>=1<<20 or p.suffix in ('.log','.out') or '/traces/' in rel:
        local[rel]=dict(bytes=p.stat().st_size,sha256=sha(p))
    else:owned.add(rel)
for n in range(6340,6349):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.py'):retain(p)

names=('20260909-6320-substitute-collapse-ranks1-50','20260909-6311-substitute-collapse-BC-gate',
       '20260909-6275-vivify-binary-BC-gate','20260909-6278-vivify-binary-ranks1-50',
       '20260909-6239-substitute-retry-ranks1-50','20260909-6332-collapse-three-wave-runtime',
       '20260909-6340-rank32-transitive-census','20260909-6345-rank32-selected-transitive')
for name in names:
    c=root/'benchmarks/runs'/name;m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('pin_sha256',m.get('hashes',{})).items():assert sha(c/rel)==digest,rel
    launches=sorted(c.glob('wave*-submission.json')) if '6332-' in name else [c/'launch.json']
    accounting={};states={};latest={}
    for p in launches:
        launch=json.loads(p.read_text());assert sha(c/'manifest.json')==launch['manifest_sha256']
        command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
        output=subprocess.check_output(command,text=True)
        accounting[p.name]=dict(launch=launch,command=command,stdout=output)
        for line in output.splitlines():
            fields=line.split('|');task=fields[0].split('_')
            if len(task)==2 and task[1].isdigit():states[int(task[1])]=fields[1]
        retain(p)
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=json.loads(p.read_text())
            if 'task' in r:
                state=classify(r,folder=='results')
                assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
                latest[r['task']]=(r,folder=='results')
                if '6332-' in name:
                    binding=m['work_oracles'][r['arm']][str(r['rank'])]
                    assert sha(root/binding['path'])==binding['sha256']
                    oracle=json.loads((root/binding['path']).read_text())
                    for key in ('certificate_sha256','certificate_bytes','verdict','instance'):assert r[key]==oracle[key],(p,key)
                    nt=lambda v:[s for s in v['solver_statistics'] if not s.startswith('c timing')]
                    assert nt(r)==nt(oracle),p
                if folder=='results':assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            else:
                assert r['exit_code']==0 and r['scratch_deleted'] and not Path(r['scratch']).exists()
                for filename,digest in r['trace_sha256'].items():
                    q=c/'traces'/filename;assert sha(q)==digest;retain(q)
            retain(p)
    outcomes={t:classify(r,final,states.get(t)) for t,(r,final) in latest.items()}
    assert all(s['status'] in ('pending_validation','checked_SAT','checked_UNSAT') for s in outcomes.values()),(name,outcomes)
    if latest:
        expected=18 if '6332-' in name else 66 if 'BC-gate' in name else 50
        assert len(latest)==expected,(name,len(latest))
    for p in c.rglob('*'):
        if p.is_file() and (p.suffix in ('.py','.sbatch') or p.name in ('manifest.json','matrix.tsv','summary.json','solver-checkpoint-6343.json')):retain(p)
    runs[name]=dict(accounting=accounting,outcomes=outcomes)
    print(name,len(outcomes),'expected',sum(s['checked'] for s in outcomes.values()),'checked',flush=True)

c=root/'benchmarks/runs/20260909-6340-rank32-transitive-census'
s=json.loads((c/'summary.json').read_text())
assert s['parent_replay'] and s['kissat_only_reductions']==[dict(pair=[-30097,936],copies=1)]
assert not s['candidate_only_reductions']
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
timing=root/'benchmarks/runs/20260909-6332-collapse-three-wave-runtime/solver-checkpoint-6343.json'
s=json.loads(timing.read_text());assert s['solver_outcomes']==18 and s['exact_per_arm_work_and_proof']
assert all(r['absolute_range_within_two_percent'] for r in s['ranks'].values())
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
commands=[(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations')]
for name,command in commands:
    p=subprocess.run(command,cwd=root,text=True,capture_output=True,check=True)
    checks[name]=dict(command=command,stdout=p.stdout,stderr=p.stderr);print(name,'PASS',flush=True)
report=root/'report-to-andrew/20260909-0530-replicas-and-census'
assert 'Overfull' not in (report/'report.log').read_text()
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},local_hash_bound=local,
    protected_campaign_hashes=prior['protected_campaign_hashes'],runs=runs,checks=checks,
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    cleanup='No manual deletion. Completed6340 runner-owned partial-proof/input cleanup verified; partial proof is not final acceptance. All live checker and selected6345 scratch untouched. Frozen pins/cpu-q/baselines unchanged.',
    failure_classification='6342 passed exact full replay/framing but incorrectly equated conflict-filtered counts to one invocation; preserved6342 and corrected with6344 plus two regression tests. No solver/proof failure.',
    next='Validate6345 selected-pair non-transitive6340 replay and partial proof before interpretation; add selected-event framing tests. All6332 solver waves exact; continue checker outcomes, not duplicate timing. Sourcefuzz/memory/newtracker approval/promotion remain open; no denied push/transfer retry or fuzz inspection.'),indent=2)+'\n')
print(target)
