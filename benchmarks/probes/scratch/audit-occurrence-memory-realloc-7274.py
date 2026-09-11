#!/usr/bin/env python3
"""Freeze complete current-memory evidence, allocator validation, and live gates."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-11-occurrence-memory-realloc-audit-7274.json'
assert not target.exists(), 'successful audit frozen'
hashes,owned = {},set()
def bind(p,expected=None,own=False):
    digest=sha(p); assert expected is None or digest == expected,p
    rel=str(p.relative_to(root)); hashes[rel]=digest
    if own: owned.add(rel)
def read(p,own=False):
    bind(p,own=own); return json.loads(p.read_text())
prior=read(root/'benchmarks/reports/2026-09-11-occurrence-evaluation-audit-7263.json')
for rel in prior['owned_paths']:
    if rel != 'PROGRESS.md': bind(root/rel,prior['hashes'][rel])
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
for rel in ('PROGRESS.md','DEVELOPMENT_PLAN.md','CLAUDE.md','docs/implementation-notes.md'):
    bind(root/rel,own=rel=='PROGRESS.md')

for stem in ('prepare-occurrence-memory-7265','submit-occurrence-memory-7265',
    'validate-occurrence-memory-7266','build-dense-realloc-7267',
    'check-dense-realloc-sanitizers-7268','check-dense-realloc-proofs-7269',
    'seal-dense-realloc-build-7270','seal-dense-realloc-build-7271',
    'prepare-dense-realloc-BC-7272','check-submit-dense-realloc-BC-7272',
    'prepare-dense-realloc-runtime-7273','check-submit-dense-realloc-runtime-7273',
    'audit-occurrence-memory-realloc-7274'):
    bind(root/f'benchmarks/probes/scratch/{stem}.py',own=True)
for stem in ('7267-dense-realloc-build','7270-dense-realloc-build',
             '7271-dense-realloc-build','7268-dense-realloc-sanitizers','7269-dense-realloc-proofs'):
    c=root/f'benchmarks/runs/20260911-{stem}'
    for p in c.rglob('*'):
        if p.is_file(): bind(p,own=p.suffix in ('.json','.log','.patch'))
build=read(root/'benchmarks/runs/20260911-7271-dense-realloc-build/manifest.json')
assert build['status']=='PASS' and len(build['changed'])==4
for rel,digest in build['files'].items(): bind(root/build['source']/rel,digest)
for rel,digest in build['binaries'].items(): bind(root/rel,digest)
for rel,digest in build['parent_objects'].items(): bind(root/rel,digest)
repair=read(root/'benchmarks/runs/20260911-7271-dense-realloc-build/support-repair.json')
assert repair['status']=='PASS'
for key in ('failed_logs','unchanged_compiled_files'):
    for rel,digest in repair[key].items(): bind(root/rel,digest)
san=read(root/'benchmarks/runs/20260911-7268-dense-realloc-sanitizers/summary.json')
proof=read(root/'benchmarks/runs/20260911-7269-dense-realloc-proofs/summary.json')
for result in (san,proof):
    assert result['status']=='PASS'
    for rel,digest in result['hashes'].items(): bind(root/rel,digest)
assert len(san['outcomes'])==10 and all(len(v)==2 for v in san['outcomes'].values())
assert (proof['checked_UNSAT'],proof['validated_SAT_models'],proof['negative_controls'])==(156,156,2)
assert proof['exact_parent_certificates'] and proof['removed_bytes']==63972
pc=root/'benchmarks/runs/20260911-7269-dense-realloc-proofs'
assert not list(pc.glob('*.cnf')) and not list(pc.glob('*.proof'))

full=root/'benchmarks/reports/2026-09-11-occurrence-full50-7262'
summary=read(full/'summary.json',own=True); bind(full/'comparison.tsv',own=True)
for rel,digest in summary['hashes'].items(): bind(root/rel,digest)
assert summary['exact_parent_work_and_proofs'] and summary['score']['records']==50
assert summary['score']['candidate_par2']==8057.066
memory=read(root/'benchmarks/runs/20260911-7265-occurrence-memory/summary-7266.json',own=True)
for rel,digest in memory['hashes'].items(): bind(root/rel,digest)
assert memory['checked_clean']==2 and [r['rank'] for r in memory['records']]==[9,25]
for row in memory['records']:
    assert row['exact_own_release_work_and_certificate']
    assert row['previous_sample']['dense_ordered_arena_capacity_bytes']==536870908
    assert row['first_peak_sample']['dense_ordered_arena_capacity_bytes']==1073741820

expected={name:v['expected'] for name,v in prior['campaigns'].items()}
expected.update({'20260911-7265-occurrence-memory':2,'20260911-7272-dense-realloc-BC':66,
                 '20260911-7273-dense-realloc-runtime':30})
new={'20260911-7265-occurrence-memory','20260911-7272-dense-realloc-BC','20260911-7273-dense-realloc-runtime'}
campaigns,jobs={},set()
for name,count in expected.items():
    c=root/'benchmarks/runs'/name; m=read(c/'manifest.json',own=name in new)
    for rel,digest in m['pin_sha256'].items():
        bind(c/rel,digest,own=name in new and (c/rel).suffix in ('.py','.tsv','.sbatch','.patch'))
    for p in c.iterdir():
        if p.is_file() and (p.name=='launch.json' or p.name.endswith('-submission.json')):
            launch=read(p,own=name in new); jobs.add(str(launch['job_id']))
            assert launch['manifest_sha256']==sha(c/'manifest.json')
    states={}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r=read(p,own=True); task=r['task']; assert task==int(p.stem) and 0<=task<count
            state=classify(r,folder=='results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            states[task]=state
    campaigns[name]=dict(expected=count,published=len(states),checked=sum(s['checked'] for s in states.values()),records=states)
assert campaigns['20260911-7272-dense-realloc-BC']['published']==66
assert campaigns['20260911-7262-occurrence-ranks1-50']['published']==50
assert campaigns['20260911-7265-occurrence-memory']['checked']==2
for p in (root/'benchmarks/runs/20260911-7265-occurrence-memory/logs').glob('*.slot-memory.tsv'):
    bind(p,own=True)

scheduler={}
for label,command in (
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),
        '-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])):
    p=subprocess.run(command,capture_output=True,text=True,timeout=30)
    scheduler[label]=dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
    assert p.returncode==0,(label,p.stderr)

report=root/'report-to-andrew/20260911-0503-occurrence-memory'
for name in ('report.tex','report.pdf','report.log'): bind(report/name)
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
bind(root/'benchmarks/reports/2026-09-11-occurrence-memory-realloc-7274.md',own=True)
commands=[[sys.executable,'-m','unittest','discover','-s',d,'-p','test_*.py'] for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable,f'tools/docs/{name}.py'] for name in
    ('check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary',
     'check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,f'benchmarks/probes/scratch/{name}.py','--self-test'] for name in
    ('validate-occurrence-memory-7266','check-submit-dense-realloc-runtime-7273','validate-occurrence-full50-7262')]
commands += [[sys.executable,f'benchmarks/probes/scratch/{name}.py'] for name in
    ('check-submit-occurrence-BC-7256','check-submit-occurrence-runtime-7257','validate-occurrence-profiles-7259',
     'validate-occurrence-full50-7262','validate-occurrence-memory-7266',
     'check-submit-dense-realloc-BC-7272','check-submit-dense-realloc-runtime-7273')]
checks={}
for command in commands:
    p=subprocess.run(command,cwd=root,capture_output=True,text=True)
    checks[' '.join(command)]=dict(exit_code=p.returncode,output=p.stdout+p.stderr)
    assert p.returncode==0,(command,p.stdout,p.stderr)
    print('PASS',' '.join(command),flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,
        owned_paths=sorted(owned),campaigns=campaigns,scheduler=scheduler,checks=checks,
        previous_turn='No convergence progress: user-requested read-only bookkeeping audit confirmed retained diagnostics and earlier no-net-gain ablation. Current turn seals all50, establishes current memory-growth evidence and validates/launches the allocator repair.',
        decision='Current largest unshelved excess-memory witnesses9/25 justify realloc experiment; exact-capacity implementation passes full fixed validation and all66 BC solver outcomes. Three paired9/25/31/35/38 waves underway. No runtime/RSS gain or promotion assumed.',
        newly_deleted_bytes=63972,
        cleanup='Only own7269 fixed original-CNF/proof files removed, receipts and logs retained. Checked benchmark records require certificate and derived-artifact deletion; active checker scratch remains owned by its runner. No existing campaign, successful evidence or submitted pin changed.',
        non_solver_errors='7267 missing clone support routes caused22 harness failures and2 skips per suite.7270 suites green but sealer expected older CTest success wording.7271 complete validation succeeds and binds unchanged compiled binaries/objects plus both failed attempts. TeX overfull list corrected before final compilation/freeze.',
        limitation='Original runtime target, memory parity, current-source Claude-owned fuzz, remaining independent checks/residue and consolidation remain open. No denied fuzz access/transfer or push retry.'),out,indent=2)
print(target)
