#!/usr/bin/env python3
"""Freeze source, live-job and validation evidence without blocking on checks."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from runtime_summary import counters
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1048576),b''):h.update(block)
    return h.hexdigest()
def run(args):return subprocess.run(args,cwd=root,capture_output=True,text=True,check=True).stdout
jobs={5395:1669252,5421:1669878,5422:1669812,5423:1669887,5424:1669919,
      5425:1669961,5426:None,5428:1669968,5429:1669969,5430:None,
      5433:1669971,5434:1669972,5435:1669973}
queue=run(['squeue','-u','hwu','-h','-o','%i %T %j']).splitlines()
for line in queue:assert int(line.split()[0].split('_')[0]) in set(jobs.values())|{1668242},line
campaigns=[];full_views={};checker_memory=[]
for number,job in jobs.items():
    c=next((root/'benchmarks/runs').glob(f'20260907-{number}-*'))
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m.get('hashes',m.get('pin_sha256',{})).items():assert sha(c/rel)==digest,(number,rel)
    for rel,digest in m.get('binaries',{}).items():assert sha(root/rel)==digest
    for rel,digest in m.get('files',{}).items():assert sha(root/m['source']/rel)==digest
    for section,prefix in [('logs','logs'),('patches',''),('assembly','')]:
        for rel,digest in m.get(section,{}).items():assert sha(c/prefix/rel)==digest
    records=sorted((c/'results').glob('*.json'));provisional=sorted((c/'provisional').glob('*.json'))
    rows={}
    for kind,paths in [('provisional',provisional),('results',records)]:
        for p in paths:
            r=json.loads(p.read_text())
            if number in (5424,5433):
                assert r['exit_code']==0 and r['scratch_deleted']
                for name,digest in r['trace_sha256'].items():assert sha(c/'traces'/name)==digest
                continue
            assert r['expected_match'] and r['solve']['exit_code'] in (10,20) and not r['solve']['timed_out']
            if kind=='results':
                assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
                assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
                check=r['model_check'] if r['solve']['exit_code']==10 else r['proof_check']
                assert check['exit_code']==0 and not check['timed_out']
                if number==5395 and r['proof_check']:
                    checker_memory.append(dict(rank=r['rank'],arm=r['arm'],peak_rss_kb=check['peak_rss_kb'],seconds=check['wall_seconds'],record_sha256=sha(p)))
            rows[r['task']]=dict(rank=r['rank'],arm=r['arm'],expected=r['expected'],solve=r['solve'],work=counters(r),
                verification='checked-clean' if kind=='results' else 'pending',record=str(p.relative_to(root)),sha256=sha(p))
    residue=[str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and p.suffix in ('.cnf','.drat','.lrat','.frat','.grat')]
    assert not residue,residue
    state=run(['sacct','-j',str(job),'-X','--format=JobID,State,ExitCode','--parsable2']).splitlines() if job else None
    campaigns.append(dict(campaign=str(c.relative_to(root)),job=job,slurm_state=state,manifest_sha256=sha(c/'manifest.json'),
        final_records=len(records),result_sha256={p.name:sha(p) for p in records},
        provisional_sha256={p.name:sha(p) for p in provisional},
        summary_sha256={p.name:sha(p) for p in c.glob('*summary.json')},repository_residue=residue,
        retention='Active campaign or unique source/fixture/measurement authority; preserve all pins, runners and local raw evidence.'))
    if number in (5395,5423,5434):full_views[str(number)]=dict(solver_records=len(rows),checked_clean=len(records),rows=rows,
        limitation='Incomplete views do not establish an aggregate or acceptance; pending expected results guide dependent work.')
checks={name:run(['python3',str(root/'tools/docs'/(name+'.py'))]) for name in
        ('check-doc-size','check-progress','check-section-refs','check-option-coverage','check-rule-policy-boundary')}
log=root/'build/release/Testing/Temporary/LastTest.log'
assert log.read_text().count('Test Passed.')==77
target=root/'benchmarks/reports/2026-09-07-root-entry-main-ctest.log'
assert not target.exists();shutil.copy2(log,target)
report=root/'report-to-andrew/20260907-2055-root-entry'
assert 'Overfull' not in (report/'report.log').read_text()
for n in (1,2):assert 'Output written on report.pdf' in (report/f'compile-{n}.log').read_text()
assert not run(['git','diff','--','src','tests'])
result=dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),queue=queue,campaigns=campaigns,
    full_views=full_views,previous_proof_checker_memory=checker_memory,checks=checks,
    main_ctest=dict(passed=77,seconds=6.38,log=str(target.relative_to(root)),sha256=sha(target)),
    progress_lines=len((root/'PROGRESS.md').read_text().splitlines()),
    source_status='Production unchanged; independent metadata 5426 and semantic root-entry 5430 remain UNFUZZED and unpromoted.',
    workflow_change='User approval requested for separate future solver/checker jobs; no contract or runner change made.',
    manual_removed_bytes=0,uncertain_deletions=[],
    andrew_report=dict(directory=str(report.relative_to(root)),compilations=2,tex_sha256=sha(report/'report.tex'),pdf_sha256=sha(report/'report.pdf')),
    scope='Fixed tests, normal runtime and bounded diagnostics only; no new fuzz campaign/artifact inspected. Current jobs and user allocation untouched; prior push rejection remains in force.')
p=root/'benchmarks/reports/2026-09-07-root-entry-validation.json'
assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print('PASS: source/pin identity, checked/provisional separation, scoped residue audit, docs and twice-compiled report')
