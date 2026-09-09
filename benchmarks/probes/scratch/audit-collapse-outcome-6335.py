#!/usr/bin/env python3
"""Seal current-source localization, full50 scoring, and paired wave authority."""
from pathlib import Path
source=Path(__file__).with_name('audit-collapse-launch-6325.py')
code=source.read_text()
for old,new in (
    ('2026-09-09-collapse-launch-audit-6325.json','2026-09-09-collapse-outcome-audit-6335.json'),
    ('2026-09-09-rank32-later-audit-6317.json','2026-09-09-collapse-launch-audit-6325.json'),
    ('range(6319,6327)','range(6327,6337)'),
    ('20260909-0439-collapse-full50-launch','20260909-0452-collapse-outcome'),
    ('Validate6321 with6323 when terminal; score6320 only with all50 outcomes. Continue pending checks asynchronously. No promotion or tracker/FlatLists approval inferred; no fuzz inspection or denied push/transfer retry.',
     'Current6321 validated108987868matchingdecisions then1405411split; oldBVE splitclosed. Next1368343TransitiveReduce one-extra nativebinaryremoval. Full6320all50expected1.187862567x; advance6332timing6333waves1/2 after6/12expectedarm-specificexactwork/proof,notproofchecking. No promotion; memory,sourcefuzz,tracker/FlatLists approvals open.')):
    assert old in code,old;code=code.replace(old,new)
marker='commands=[(n,'
assert code.count(marker)==1
extra="""# Current-source diagnostic build is a portable instrumentation change only.
build=root/'benchmarks/runs/20260909-6327-collapse-schedule-build'
bs=json.loads((build/'summary.json').read_text());assert bs['release_exact_parent']
for rel,digest in bs['files'].items():assert sha(root/bs['source']/rel)==digest
for rel,digest in bs['binaries'].items():assert sha(root/rel)==digest
for rel,digest in bs['logs'].items():assert sha(build/rel)==digest
assert bs['binaries']['build/pin-6327-collapse-schedule-release/tactix']=='33a94aec8df9a3be4f2a902e13d59dcc1527a412302f646140bab2ec5c4193e4'
for q in build.rglob('*'):
    if q.is_file() and q.suffix in ('.json','.patch','.log'):retain(q)
prefix=root/'benchmarks/runs/20260909-6321-collapse-rank32-prefix/summary.json'
ps=json.loads(prefix.read_text());assert ps['matching_decisions_from_start']==108987868
assert ps['decisions']['left']==[1405411,108780509,2872,33105,0]
assert ps['decisions']['right']==[1405411,108780509,2872,103431,0]
for rel,digest in ps['hashes'].items():assert sha(root/rel)==digest
retain(prefix)
for name in ('2026-09-09-collapse-complete-checkpoint-6329.json','2026-09-09-collapse-runtime-outcome.md'):retain(root/'benchmarks/reports'/name)
summary=root/'benchmarks/reports/2026-09-09-collapse-full50-6334/summary.json'
fs=json.loads(summary.read_text());assert fs['solver_outcomes']==50 and fs['ratio']==1.1878625668427034
for rel,digest in fs['hashes'].items():assert sha(root/rel)==digest
for q in summary.parent.iterdir():retain(q)
timing=root/'benchmarks/runs/20260909-6332-collapse-three-wave-runtime'
tm=json.loads((timing/'manifest.json').read_text())
for rel,digest in tm['pin_sha256'].items():assert sha(timing/rel)==digest
for binding in tm['authorities'].values():assert sha(root/binding['path'])==binding['sha256']
timing_states={};timing_waves={}
for folder in ('provisional','results'):
    for q in sorted((timing/folder).glob('*.json')):
        r=json.loads(q.read_text());state=classify(r,folder=='results')
        assert state['status'] in ('pending_validation','checked_UNSAT'),(q,state)
        binding=tm['work_oracles'][r['arm']][str(r['rank'])]
        assert sha(root/binding['path'])==binding['sha256']
        oracle=json.loads((root/binding['path']).read_text())
        for key in ('certificate_sha256','certificate_bytes','verdict','instance'):assert r[key]==oracle[key],(q,key)
        nt=lambda v:[line for line in v['solver_statistics'] if not line.startswith('c timing')]
        assert nt(r)==nt(oracle),q
        timing_states[r['task']]=state;retain(q)
for q in sorted(timing.glob('wave*-submission.json')):
    launch=json.loads(q.read_text());assert launch['manifest_sha256']==sha(timing/'manifest.json')
    command=['sacct','-n','-X','--array','-j',launch['job_id'],'-S',launch['accounting_since'],'-o','JobID,State%40,ExitCode,Submit','-P']
    timing_waves[q.name]=dict(launch=launch,accounting=dict(command=command,stdout=subprocess.check_output(command,text=True)))
    retain(q)
for q in timing.rglob('*'):
    if q.is_file() and (q.suffix in ('.py','.sbatch') or q.name in ('manifest.json','matrix.tsv')):retain(q)
runs[timing.name]=dict(states=timing_states,waves=timing_waves)
print('Timing',len(timing_states),'expected exact-arm work/proof outcomes',flush=True)
assert not (root/'benchmarks/probes/scratch/prepare-collapse-membership-6330.py').exists()
assert not (root/'benchmarks/runs/20260909-6330-collapse-rank32-membership').exists()
"""
code=code.replace(marker,extra+marker)
code=code.replace(
    "cleanup='No manual deletion; inherited and new live checker scratch untouched. Final result classification checks runner cleanup; bounded partial proof is not final certificate acceptance.',",
    "cleanup='Only obsolete unexecuted6330 helper removed; no campaign or job existed. Live checker scratch untouched. Runner-owned bounded partial proof/input cleanup verified; partial proof is not final acceptance.', report_layout='Initial audit stopped at the report Overfull assertion without publishing an audit artifact. Reworded the overflowing sentence and compiled twice; final layout checked here. No solver or proof failure.',")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
