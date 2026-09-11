#!/usr/bin/env python3
"""Freeze completed AND evidence and the independent root-collection campaign."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-11-and-collection-audit-7232.json'
assert not target.exists(), 'successful checkpoint frozen'
hashes,owned = {},set()
def bind(p,expected=None,own=False):
    digest = sha(p); assert expected is None or digest == expected,p
    rel = str(p.relative_to(root)); hashes[rel] = digest
    if own: owned.add(rel)
def bound_json(p,own=False):
    r = json.loads(p.read_text()); bind(p,own=own)
    for rel,digest in r.get('hashes',{}).items(): bind(root/rel,digest)
    return r
p = root/'benchmarks/reports/2026-09-11-root-binary-audit-7225.json'
old = json.loads(p.read_text()); bind(p)
for rel in old['owned_paths']:
    if rel != 'PROGRESS.md': bind(root/rel,old['hashes'][rel])
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
for rel in ('PROGRESS.md','DEVELOPMENT_PLAN.md','CLAUDE.md'):
    bind(root/rel,own=rel == 'PROGRESS.md')
for name in ('prepare-root-binary-runtime-7226','check-submit-root-binary-runtime-7226',
             'assess-congruence-fused-7227','localize-reduce-garbage-7228',
             'localize-reduce-garbage-pcs-7229','prepare-root-binary-profiles-7230',
             'validate-root-binary-profiles-7230','submit-root-binary-profile-wave-7230',
             'extract-root-binary-wave-7231','audit-and-collection-7232'):
    bind(root/f'benchmarks/probes/scratch/{name}.py',own=True)
assessment = bound_json(root/'benchmarks/reports/2026-09-11-congruence-fused-assessment-7227.json',own=True)
assert assessment['runtime_expected'] == 18 and assessment['profile_expected'] == 6
assert all(r['absolute_headline_stable'] for r in assessment['spreads'].values())
assert assessment['runtime']['31']['paired']['wall_seconds']['median'] > 1
assert assessment['runtime']['35']['paired']['wall_seconds']['median'] > 1
assert assessment['complete_congruence']['max'] < 1
gc = bound_json(root/'benchmarks/reports/2026-09-11-reduce-garbage-7228.json',own=True)
pcs = bound_json(root/'benchmarks/reports/2026-09-11-reduce-garbage-pcs-7229.json',own=True)
for label,r in pcs['arms'].items():
    assert r['total_period'] == gc['arms'][label]['total_period']
    assert r['selected_period'] == gc['arms'][label]['selected_period']
    assert r['self_period'] == gc['arms'][label]['branches']['no_phase_frame']['<entry self>']
    assert sum(s['self_period'] for s in r['symbols'].values()) == r['self_period']
    assert all(sum(row['period'] for row in s['instruction_rows']) == s['self_period'] for s in r['symbols'].values())
stream = bound_json(root/'benchmarks/runs/20260911-7207-congruence-fused-samples/wave2/summary.json',own=True)
assert stream['temporary_expansions_deleted_bytes'] == 1201392500
bind(root/'benchmarks/reports/2026-09-11-and-result-collection-next-7232.md',own=True)
specs = [('20260910-7167-vivify-revalidation-ranks1-50',50),
         ('20260911-7201-factor-arena-ranks1-50',50),
         ('20260911-7202-congruence-fused-BC',66),
         ('20260911-7205-congruence-fused-runtime',18),
         ('20260911-7206-congruence-fused-profiles',6),
         ('20260911-7210-rank31-retry-producer',2),
         ('20260911-7221-root-binary-BC',66),
         ('20260911-7226-root-binary-runtime',18),
         ('20260911-7230-root-binary-profiles',6)]
campaigns,jobs = {},set()
for name,count in specs:
    c = root/'benchmarks/runs'/name
    if not c.exists():
        assert name == '20260911-7230-root-binary-profiles'
        campaigns[name] = dict(created=False,reason='first exact unprofiled Rank35 pair required')
        continue
    m = json.loads((c/'manifest.json').read_text()); bind(c/'manifest.json',own=True)
    for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest)
    for group in ('dependencies','protected_campaign_hashes'):
        for rel,digest in m.get(group,{}).items(): bind(root/rel,digest)
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json','.tsv','.sbatch'):
            bind(p,own=True)
            if p.name == 'launch.json' or p.name.endswith('-submission.json'):
                launch = json.loads(p.read_text())
                assert launch['manifest_sha256'] == sha(c/'manifest.json')
                jobs.add(str(launch['job_id']))
    for p in (c/'pins').iterdir():
        if p.suffix in ('.py','.patch'): bind(p,own=True)
    rows = {}
    for folder in ('provisional','results'):
        for p in (c/folder).glob('*.json'):
            r = json.loads(p.read_text()); task = r['task']
            assert 0 <= task < count and task == int(p.stem)
            state = classify(r,folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            bind(p,own=True); rows[task] = state
    campaigns[name] = dict(created=True,expected=count,published=len(rows),checked=sum(r['checked'] for r in rows.values()),records=rows)
assert campaigns[specs[0][0]]['checked'] == 50
assert campaigns['20260911-7221-root-binary-BC']['published'] == 66
scheduler = {}
for name,command in (
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),'-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])):
    p = subprocess.run(command,capture_output=True,text=True,timeout=30)
    scheduler[name] = dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
    if name == 'queue': assert p.returncode == 0,p.stderr
report = root/'report-to-andrew/20260911-0151-and-collection'
for name in ('report.tex','report.pdf','report.log'): bind(report/name)
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
commands = [[sys.executable,'-m','unittest','discover','-s',d,'-p','test_*.py'] for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable,'tools/docs/'+name+'.py'] for name in (
    'check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary',
    'check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name+'.py','--self-test'] for name in (
    'assess-congruence-fused-7227','localize-reduce-garbage-7228','check-submit-root-binary-runtime-7226')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name+'.py'] for name in (
    'validate-vivify-revalidation-full50-7168','validate-factor-arena-full50-7201',
    'check-submit-congruence-fused-runtime-7205','validate-congruence-fused-profiles-7206',
    'check-submit-root-binary-BC-7221','check-submit-root-binary-runtime-7226')]
if campaigns['20260911-7230-root-binary-profiles']['created']:
    commands.append([sys.executable,'benchmarks/probes/scratch/validate-root-binary-profiles-7230.py'])
checks = {}
for command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode,output=p.stdout+p.stderr)
    assert p.returncode == 0,(command,p.stdout,p.stderr)
    print('PASS',' '.join(command),flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,owned_paths=sorted(owned),
        campaigns=campaigns,scheduler=scheduler,checks=checks,
        previous_turn='Progress:7218 local gates,7221 submission and full50 proof closure committed63b07992. Current turn advances completed AND interpretation, larger GC instruction attribution and root-binary runtime submission.',
        decision='AND local phase saving does not justify net-runtime promotion. Preserve all pairs. Continue7226/7230 independently, and prove forwarding-sentinel/prefix equivalence before an isolated sparse-GC watch-filter implementation.',
        newly_deleted_bytes=1201392500,cleanup='Only own verified wave2 profile expansion removed; compressed originals, extracted streams and checker residue preserved.',
        limitation='No new solver source, production promotion, native denominator rerun, source-fuzz acceptance or denied push/access retry. Missing scheduler observation is not a terminal result. Target, memory, remaining proofs and consolidation open.'),out,indent=2)
print(target)
for name,r in campaigns.items(): print(name,r.get('published'),r.get('checked'))
