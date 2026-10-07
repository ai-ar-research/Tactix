#!/usr/bin/env python3
"""SLURM-only, sequential bounded census and independent RUP; never submits."""
import hashlib,json,os,platform,subprocess,time
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2]
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(65536),b''):h.update(chunk)
    return h.hexdigest()
m=json.loads((HERE/'launch-manifest.json').read_text())
assert os.environ['SLURM_ARRAY_TASK_ID']=='0'
for b in m['bindings']:assert digest(ROOT/b['path'])==b['sha256'],b['path']
authority=json.loads((ROOT/m['input']['authority_path']).read_text())
assert authority[m['input']['authority_field']][m['input']['authority_key']]==m['input']['sha256']
result=HERE/('result-'+os.environ['SLURM_ARRAY_JOB_ID']+'-0');result.mkdir()
inp=m['input'];limits=m['limits'];selection=m['selection']
census=[str(HERE/'pins/census'),str(ROOT/inp['path']),str(result/'census.json'),inp['sha256'],str(inp['variables']),str(inp['clauses']),str(inp['decoded_bytes']),str(selection['seed_or']),str(limits['census_seconds']),str(limits['query_work']),str(limits['allocation_bytes']),str(inp['compressed_bytes']),str(selection['seed_blocker']),str(selection['authorized_domain_begin'])]
checker=['python3',str(HERE/'pins/check_original_rup.py'),str(ROOT/inp['path']),str(result/'census.json'),str(result/'proof-check.json'),inp['sha256'],str(inp['variables']),str(inp['clauses']),str(inp['decoded_bytes'])]
(result/'commands.json').write_text(json.dumps({'census':census,'checker_base':checker},indent=2)+'\n')
started=time.monotonic();deadline=started+limits['combined_process_seconds']
# /usr/bin/time and its child share a process group, so a timed-out census or
# checker must be terminated together, not leave an orphan consuming storage.
def execute(command,name,allowed):
    import signal
    with (result/(name+'-stdout.log')).open('wb') as out,(result/(name+'-stderr.log')).open('wb') as err:
        process=subprocess.Popen(['/usr/bin/time','-f','wall_seconds=%e\npeak_rss_kb=%M','-o',str(result/(name+'-resource.txt')),*command],stdout=out,stderr=err,start_new_session=True)
        try:return process.wait(timeout=allowed)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid,signal.SIGKILL);process.wait();return 124
census_rc=execute(census,'census',min(limits['census_process_seconds'],max(.001,deadline-time.monotonic())))
checker_rc=None
if census_rc==0 and (result/'census.json').exists():
    c=json.loads((result/'census.json').read_text())
    remaining=deadline-time.monotonic()
    if c['query_complete'] and remaining>0.05:
        effective=[*checker,'--seconds',str(min(29,remaining-.05)),'--work',str(limits['checker_work'])]
        (result/'checker-command.json').write_text(json.dumps(effective,indent=2)+'\n')
        checker_rc=execute(effective,'checker',remaining)
outcome={'job':os.environ['SLURM_ARRAY_JOB_ID'],'host':platform.node(),'manifest_sha256':digest(HERE/'launch-manifest.json'),'census_exit':census_rc,'checker_exit':checker_rc,'combined_wall_seconds':time.monotonic()-started,'solver_run':False,'native_admission':False,'model_validation':'not_applicable_no_solver','gate':'not_admitted'}
if (result/'census.json').exists():
    c=json.loads((result/'census.json').read_text());outcome.update(input_hash_verified=c['input_hash_complete'] and c['input_file_sha256']==inp['sha256'],source_complete=c['source_complete'],query_complete=c['query_complete'],census_status=c['status'],proposed_tuples=len(c['aligned_tuples']))
if checker_rc==0 and (result/'proof-check.json').exists():
    p=json.loads((result/'proof-check.json').read_text());outcome.update(proof_validation=p['status'],novel_checked_tuples=p['novel_checked_tuples'])
    if p['status']=='checked_useful_deduction':outcome['gate']='checked_standalone_deduction_requires_root_review'
else:outcome['proof_validation']='not_completed'
(result/'outcome.json').write_text(json.dumps(outcome,indent=2)+'\n')
(result/'SHA256SUMS').write_text(''.join(digest(p)+'  '+p.name+'\n' for p in sorted(result.iterdir()) if p.is_file() and p.name!='SHA256SUMS'))
raise SystemExit(census_rc or checker_rc or 0)
