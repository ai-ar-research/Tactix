#!/usr/bin/env python3
"""Audit explicit non-fuzz authorities only; the background fuzz owner is opaque."""
import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[3]
jobs={5423:1669887,5429:1669969,5434:1669972,5435:1669973,
      5437:1670050,5440:1670118,5441:1670119,5445:None,
      5447:1670198,5448:1670199,5449:1670217,5450:None,
      5453:1670291,5454:1670292,5455:1670293}
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()
def run(args):
    return subprocess.run(args,cwd=root,check=True,capture_output=True,text=True).stdout
queue=run(['squeue','-j',','.join(str(j) for j in jobs.values() if j),'-h','-o','%i %T %j']).splitlines()
campaigns=[]
for number,job in jobs.items():
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('hashes',m.get('pin_sha256',{})).items():assert sha(c/rel)==digest,(number,rel)
    for rel,digest in m.get('binaries',{}).items():assert sha(root/rel)==digest
    for rel,digest in m.get('files',{}).items():assert sha(root/m['source']/rel)==digest
    for section,prefix in [('logs','logs'),('patches','')]:
        for rel,digest in m.get(section,{}).items():assert sha(c/prefix/rel)==digest
    results=sorted((c/'results').glob('*.json'))
    provisional=sorted((c/'provisional').glob('*.json'))
    tasks={}
    for kind,paths in [('provisional',provisional),('results',results)]:
        for p in paths:
            r=json.loads(p.read_text())
            if number in (5437,5449,5453):
                assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']
                for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest
                continue
            assert r['expected_match'] and r['solve']['exit_code'] in (10,20) and not r['solve']['timed_out']
            if kind=='results':
                assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
                assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
                check=r['model_check'] if r['solve']['exit_code']==10 else r['proof_check']
                assert check['exit_code']==0 and not check['timed_out']
            tasks[r['task']]=kind
    residue=[str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and p.suffix in ('.cnf','.drat','.lrat','.frat','.grat')]
    assert not residue,residue
    state=run(['sacct','-j',str(job),'-X','--format=JobID,State,ExitCode','--parsable2']).splitlines() if job else None
    campaigns.append(dict(campaign=str(c.relative_to(root)),job=job,slurm_state=state,
        manifest_sha256=sha(c/'manifest.json'),final_records=len(results),solver_tasks=len(tasks),
        result_sha256={p.name:sha(p) for p in results},
        provisional_sha256={p.name:sha(p) for p in provisional},
        summary_sha256={p.name:sha(p) for p in c.glob('*summary.json')},repository_residue=residue,
        retention='Unique source, fixed fixture, causal or measurement authority; retain pins/runners and raw evidence.'))
checks={name:run(['python3',str(root/'tools/docs'/(name+'.py'))]) for name in
        ('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary')}
spec=importlib.util.spec_from_file_location('progress_check',root/'tools/docs/check-progress.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
errors=[];warnings=[];notes=[]
module.check_citations(errors,warnings,notes)
assert not errors,errors
checks['check-progress-scoped']=dict(citation_errors=errors,citation_warnings=warnings,
    limitation='Full queue/build scans deferred because they inspect independently owned fuzz resources; only the explicitly listed non-fuzz campaigns are audited.')
main=root/'build/release/Testing/Temporary/LastTest.log'
assert main.read_text().count('Test Passed.')==77
target=root/'benchmarks/reports/2026-09-07-eliminate-budget-main-ctest.log'
assert not target.exists();shutil.copy2(main,target)
report=root/'report-to-andrew/20260907-2255-eliminate-budget'
assert 'Output written on report.pdf' in (report/'report.log').read_text()
assert 'Overfull' not in (report/'report.log').read_text()
assert not run(['git','diff','--','src','tests'])
result=dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),queue=queue,
    campaigns=campaigns,checks=checks,main_ctest=dict(passed=77,seconds=5.32,log=str(target.relative_to(root)),sha256=sha(target)),
    progress_lines=len((root/'PROGRESS.md').read_text().splitlines()),
    source_status='Production unchanged. Candidate 5450 passes 81/81 twice and 80/80 trace; causal prefix extends to 7918162 rows; runtime/regression queued; 5445/5450 not covered by ongoing predecessor fuzz.',
    fuzz_owner='Session 30094 pending; terminal response only, no fuzz jobs/artifacts inspected.',
    manual_removed_bytes=0,reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',
    andrew_report=dict(directory=str(report.relative_to(root)),compilations=2,tex_sha256=sha(report/'report.tex'),pdf_sha256=sha(report/'report.pdf')),
    constraints='No promotion, push, or separate solve/check runner change. Existing proof checking and periodic fuzz remain background work. User allocation untouched.')
p=root/'benchmarks/reports/2026-09-07-eliminate-budget-validation.json'
assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print('PASS: explicit non-fuzz pins/results/residue, docs, production tests and report')
