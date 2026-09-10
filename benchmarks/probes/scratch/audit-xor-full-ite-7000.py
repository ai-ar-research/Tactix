#!/usr/bin/env python3
"""Read-only final evidence audit; freeze one explicit owned landing set."""
import csv
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha, score
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-10-xor-full-ite-audit-7000.json'
assert not target.exists()
prior_path = root/'benchmarks/reports/2026-09-10-factor-xor-final-audit-6978.json'
prior = json.loads(prior_path.read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in prior[group].items():
        if rel != 'PROGRESS.md': assert sha(root/rel) == digest,rel
assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)
owned = {'PROGRESS.md','benchmarks/reports/2026-09-10-xor-full50-and-ite-pair.md'}
evidence = {}
def own(p):
    assert p.is_file(),p
    owned.add(str(p.relative_to(root)))
def read(p, stage=True):
    evidence[str(p.relative_to(root))] = sha(p)
    if stage: own(p)
    return json.loads(p.read_text())
def bind(mapping):
    for rel,digest in mapping.items():
        assert sha(root/rel) == digest,rel
        evidence[rel] = digest
for number in range(6980,7002):
    for p in (root/'benchmarks/probes/scratch').glob(f'*-{number}.*'): own(p)
ite = root/'benchmarks/runs/20260910-6969-ite-pair-hoist-build'
build = read(ite/'manifest.json')
for rel,digest in build['files'].items(): assert sha(root/build['source']/rel)==digest,rel
bind(build['binaries'])
for rel,digest in build['patches'].items(): assert sha(ite/rel)==digest; own(ite/rel)
subprocess.run(['git','apply','--check','--unidiff-zero',
                '--directory=.claude/worktrees/runtime-reduce-watermark-6730',
                str(ite/'incremental.patch')],cwd=root,check=True)
for rel,digest in build['logs'].items(): assert sha(ite/rel)==digest; own(ite/rel)
for name in ('control-6985.json','budget-control-6988.json','kernel-code-6998.json','fixture-classification-6999.json'):
    r = read(ite/name); bind(r['hashes'])
    if 'outputs' in r: assert r['outputs']['previous'] == r['outputs']['candidate']
    if 'selected_bodies_identical' in r: assert r['selected_bodies_identical']
for p in (ite/'logs').glob('*.json'): own(p)
proof = root/'benchmarks/runs/20260910-6990-ite-pair-proofs'
p = read(proof/'summary.json'); bind(p['hashes'])
assert p['checked']==384 and p['LRAT_cake_lpr']==p['DRAT_drat_trim']==192 and p['negative_controls']==2
fixed_proof_bytes = p['removed_bytes']
assert not list(proof.glob('*.cnf')) and not list(proof.glob('*.proof'))
for log in (proof/'logs').glob('*.json'): own(log)
full = root/'benchmarks/runs/20260910-6980-xor-presence-ranks1-50'
full_report = root/'benchmarks/reports/2026-09-10-xor-presence-full50-6982'
s = read(full_report/'summary.json'); bind(s['hashes']); own(full_report/'comparison.tsv')
assert s['groups']['all50']['candidate']==8300.569 and not s['changed_certificate_ranks'] and not s['changed_non_timing_stat_ranks']
full_score = score(full)
assert full_score['expected_solves']==50 and full_score['fixed_kissat_seconds']==7239.010
bc = root/'benchmarks/runs/20260910-6991-ite-pair-BC-gate'
b = read(bc/'summary-6993.json'); bind(b['hashes'])
assert b['solver_records']==66 and len(b['pairs'])==33
assert all(r['reported_work_equal'] and r['certificate_equal'] for r in b['pairs'])
runtime = root/'benchmarks/runs/20260910-6994-ite-pair-runtime'
timing = read(runtime/'solver-checkpoint-6996.json'); bind(timing['hashes'])
assert timing['solver_outcomes']==18 and timing['exact_per_arm_work_and_proof']
price = read(runtime/'timer-cost-6997.json'); bind(price['hashes'])
assert price['solved']==dict(candidate=9,previous=9,SAT=0,UNSAT=18,timeouts=0,solve_losses=0)
campaigns = set(prior['campaign_states']) | {str(c.relative_to(root)) for c in (full,bc,runtime)}
states = {}; owners = {}; launches = {}
for rel in sorted(campaigns):
    c = root/rel; m = read(c/'manifest.json')
    if c in (full,bc,runtime):
        contract = m['resource_contract']
        for key,value in dict(cpus_per_task=12,memory_mib=48000,solve_timeout=1200,
                              checker_timeout=16000,one_solver_per_task=True).items():
            assert contract[key]==value,(c,key,contract[key])
        assert contract['partition']=='cpu-dense-hwulab-q'
        batch = (c/'array.sbatch').read_text()
        assert '--nodelist' not in batch and '--constraint' not in batch
        assert all('%' not in line for line in batch.splitlines() if '--array=' in line)
    for path,digest in m['pin_sha256'].items():
        assert sha(c/path)==digest,(rel,path)
        if Path(path).suffix in ('.py','.patch','.tsv','.sbatch'): own(c/path)
    for q in c.glob('*.json'):
        if 'submission' not in q.name and 'launch' not in q.name: continue
        launch = read(q)
        if 'job_id' in launch:
            owners[str(launch['job_id'])] = rel
            launches[str(q.relative_to(root))] = launch
    with (c/'matrix.tsv').open() as stream: matrix = list(csv.DictReader(stream,delimiter='\t'))
    records = {}
    for folder in ('provisional','results'):
        for q in sorted((c/folder).glob('*.json')):
            r = read(q); task = r['task']; row = matrix[task]
            assert task==int(q.stem) and r['rank']==int(row['rank'])
            for key in ('arm','instance','expected'): assert r[key]==row[key],(q,key)
            state = classify(r,folder=='results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(q,state)
            if folder=='results':
                assert state['checked'] and state['clean']
                assert r['binary_sha256']==m['arms'][r['arm']]['sha256']
            if task in records:
                old = json.loads((root/records[task]['path']).read_text())
                for key in ('solve','certificate_sha256','certificate_bytes'): assert r[key]==old[key],(q,key)
            records[task] = dict(path=str(q.relative_to(root)),state=state)
    assert len(records)==len(matrix),(rel,len(records),len(matrix))
    states[rel] = records
command = ['squeue','-h','-u','hwu','-o','%i|%j|%T|%P|%C|%m']
queue = subprocess.check_output(command,text=True); mapping = {}
for line in queue.splitlines():
    identity = line.split('|')[0]; job = identity.split('_')[0]
    if job=='1668242': continue
    assert job in owners,('unmapped active job',line)
    mapping[identity] = dict(row=line,owner=owners[job])
new_jobs = sorted(j for j,rel in owners.items() if rel in {str(c.relative_to(root)) for c in (full,bc,runtime)})
command_acct = ['sacct','-X','-j',','.join(new_jobs),'--starttime','2026-09-10T00:00:00','-n','-P','--format=JobID,State,Elapsed,ExitCode,AllocCPUS,ReqMem,Partition']
accounting = subprocess.check_output(command_acct,text=True)
for job in new_jobs: assert job in accounting,job
monitor = subprocess.check_output(['ps','-p','271019','-o','pid=,comm=,args='],text=True)
assert 'session_deadline.py' in monitor and '2026-09-10T06:00:00-04:00' in monitor
checks = {}
commands = [(n,['python3','-m','unittest','discover','-s','tools/'+n,'-p','test_*.py']) for n in ('bench','slurm')]
commands += [(n,['python3','tools/docs/'+n+'.py']) for n in ('check-doc-size','check-progress','check-rule-policy-boundary','check-proof-rule-sync','check-section-refs','check-citations','check-option-coverage')]
for name,command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True,check=True)
    checks[name] = dict(command=command,stdout=p.stdout,stderr=p.stderr)
    print(name,'PASS',flush=True)
report = root/'report-to-andrew/20260910-0545-xor-full-ite'
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
assert 'PENDING' not in (report/'report.tex').read_text()
assert 'PENDING' not in (root/'benchmarks/reports/2026-09-10-xor-full50-and-ite-pair.md').read_text()
out = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    prior_audit=dict(path=str(prior_path.relative_to(root)),sha256=sha(prior_path)),
    owned_hashes={rel:sha(root/rel) for rel in sorted(owned)},
    protected_campaign_hashes=prior['protected_campaign_hashes'], evidence=evidence,
    campaign_states=states,launches=launches,checks=checks,deadline_monitor=monitor,
    accounting=dict(queue=queue,mapping=mapping,sacct_command=command_acct,sacct=accounting),
    andrew_report={str((report/n).relative_to(root)):sha(report/n) for n in ('report.tex','report.pdf')},
    full50_score=full_score,cleanup=dict(fixed_proof_bytes=fixed_proof_bytes,
        note='Only6990 verified regenerable fixed CNF/proofs and negative controls removed by their owning checker helper. Generators, logs, objects, pins, original failed test fixtures and all live checker scratch preserved. No fuzz access.'),
    limitation='XOR6950 full50 aggregate neutral, all50 exact work/certificates; no adoption or global speedup claim. ITE6969 independent tested/measured candidate, see complete paired timing decision in report. Current6730, production and fixed Kissat authority unchanged; source fuzz, scheduling/CDCL and memory parity remain open. Proofs asynchronous; no push/transfer retry, no Rank21 probes, no new tasks after06:00EDT.')
with target.open('x') as stream: json.dump(out,stream,indent=2); stream.write('\n')
print(target)
for rel,records in states.items(): print(rel,len(records),'published',sum(v['state']['checked'] for v in records.values()),'checked')
