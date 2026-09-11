#!/usr/bin/env python3
"""Freeze the validated collector increment without promoting it or awaiting proofs."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root/'benchmarks/reports/2026-09-11-root-binary-audit-7225.json'
assert not target.exists(), 'successful audit frozen'
hashes,owned = {},set()
def bind(p,expected=None,own=False):
    digest = sha(p)
    assert expected is None or digest == expected,p
    rel = str(p.relative_to(root)); hashes[rel] = digest
    if own: owned.add(rel)
def bound_json(p,own=False):
    value = json.loads(p.read_text()); bind(p,own=own)
    for rel,digest in value.get('hashes',{}).items(): bind(root/rel,digest)
    return value
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
old = json.loads((root/'benchmarks/reports/2026-09-11-convergence-transfer-checkpoint-7214.json').read_text())
bind(root/'benchmarks/reports/2026-09-11-convergence-transfer-checkpoint-7214.json')
for rel in old['owned_paths']:
    if rel != 'PROGRESS.md': bind(root/rel,old['hashes'][rel])
for rel in ('PROGRESS.md','DEVELOPMENT_PLAN.md','CLAUDE.md'):
    bind(root/rel,own=rel == 'PROGRESS.md')
for name in ('assess-factor-transfer-controls-7215','localize-reduce-envelope-7216',
             'localize-reduce-collection-7217','build-root-binary-7218',
             'check-root-binary-proofs-7219','check-root-binary-sanitizers-7220',
             'prepare-root-binary-BC-7221','check-submit-root-binary-BC-7221',
             'check-root-binary-proof-control-7222','check-root-binary-proof-control-7224',
             'audit-root-binary-checkpoint-7225'):
    bind(root/f'benchmarks/probes/scratch/{name}.py',own=True)
for number in (7219,7222,7224):
    bind(root/f'benchmarks/probes/scratch/test-root-binary-proof-{number}.cpp',own=True)
for name in ('factor-transfer-assessment-7215','reduce-envelope-7216','reduce-collection-7217'):
    bound_json(root/f'benchmarks/reports/2026-09-11-{name}.json',own=True)
bind(root/'benchmarks/reports/2026-09-11-root-binary-checkpoint-7225.md',own=True)
build = root/'benchmarks/runs/20260911-7218-root-binary-build'
m = json.loads((build/'manifest.json').read_text()); bind(build/'manifest.json',own=True)
for rel,digest in m['files'].items(): bind(root/m['source']/rel,digest)
for rel,digest in m['binaries'].items(): bind(root/rel,digest)
assert m['binaries']['build/pin-7218-root-binary-release/tactix'] == m['binaries']['build/pin-7218-root-binary-bc/tactix']
for p in build.rglob('*'):
    if p.is_file(): bind(p,own=True)
proof = bound_json(root/'benchmarks/runs/20260911-7224-root-binary-proof-control/summary.json',own=True)
assert proof['status'] == 'PASS' and proof['checked_UNSAT'] == proof['validated_SAT_models'] == 216
assert proof['negative_controls'] == 2 and proof['exact_parent_certificates']
san = bound_json(root/'benchmarks/runs/20260911-7220-root-binary-sanitizers/summary.json',own=True)
assert san['status'] == 'PASS' and len(san['outcomes']) == 5
assert all(len(v) == 2 and v[0] == v[1] for v in san['outcomes'].values())
failures = {}
for stem in ('7219-root-binary-proofs','7222-root-binary-proof-control','7224-root-binary-proof-control','7220-root-binary-sanitizers'):
    c = root/f'benchmarks/runs/20260911-{stem}'
    for p in c.rglob('*'):
        if p.is_file(): bind(p,own=p.suffix == '.json')
    if stem.startswith(('7219','7222')):
        assert not (c/'summary.json').exists()
        p = c/'logs/previous-1-0-1-0-1-0-fixture.json'
        r = json.loads(p.read_text()); assert r['returncode'] != 0 and 'Assertion' in r['stderr']
        failures[stem] = dict(log=str(p.relative_to(root)),diagnosis='invalid full-solver fixture omitted inverse import permutation; fixed only in7224',
            retained_proofs=[dict(path=str(q.relative_to(root)),bytes=q.stat().st_size,sha256=sha(q)) for q in c.glob('*.proof')])
replay = bound_json(root/'benchmarks/runs/20260911-7210-rank31-retry-producer/solver-summary-7211.json',own=True)
marks = {a:[r for r in rows if r['conflicts'] < 5187498] for a,rows in replay['producers'].items()}
assert len(marks['candidate']) == 40 and len(marks['previous']) == 42
assert marks['previous'][:40] == marks['candidate']
extra = marks['previous'][40:]
assert all(r['conflicts'] == 4871313 and r['source'] == 'delete-clause' and r['value'] == r['retired'] == 0 for r in extra)
p = root/'benchmarks/runs/20260911-7210-rank31-retry-producer/traces/previous.macro.tsv'; bind(p)
macro = [line.split('\t') for line in p.read_text().splitlines() if '\t4871313\t' in line]
assert len(macro) == 2 and [r[2] for r in macro] == ['reduce_begin','reduce_end']
assert int(macro[1][1]) == int(macro[0][1])+1
profiles = {}
for wave in (0,1):
    c = root/f'benchmarks/runs/20260911-7207-congruence-fused-samples/wave{wave}'
    profiles[wave] = bound_json(c/'summary.json',own=True)['complete_congruence_ratio']
specs = [('20260910-7167-vivify-revalidation-ranks1-50',50),
         ('20260911-7201-factor-arena-ranks1-50',50),
         ('20260911-7202-congruence-fused-BC',66),
         ('20260911-7205-congruence-fused-runtime',18),
         ('20260911-7206-congruence-fused-profiles',6),
         ('20260911-7210-rank31-retry-producer',2),
         ('20260911-7212-factor-arena-transfer-controls',18),
         ('20260911-7221-root-binary-BC',66)]
campaigns,jobs = {},set()
for name,count in specs:
    c = root/'benchmarks/runs'/name
    m = json.loads((c/'manifest.json').read_text()); bind(c/'manifest.json',own=True)
    for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest)
    for rel,digest in m.get('dependencies',{}).items(): bind(root/rel,digest)
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json','.tsv','.sbatch'):
            bind(p,own=True)
            if p.name == 'launch.json' or p.name.endswith('-submission.json'):
                launch = json.loads(p.read_text())
                assert launch['manifest_sha256'] == sha(c/'manifest.json')
                jobs.add(str(launch['job_id']))
    for p in (c/'pins').iterdir():
        if p.suffix in ('.py','.patch'): bind(p,own=True)
    records = {}
    for folder in ('provisional','results'):
        for p in (c/folder).glob('*.json'):
            r = json.loads(p.read_text()); task = r['task']
            assert 0 <= task < count and task == int(p.stem)
            state = classify(r,folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            bind(p,own=True); records[task] = state
    campaigns[name] = dict(expected_tasks=count,published=len(records),checked=sum(r['checked'] for r in records.values()),
        records=records,missing_result_interpretation='unknown/pending; consult queue or accounting, never assume terminal')
assert campaigns[specs[0][0]]['checked'] == 50
scheduler = {}
for label,command in (
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),'-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])):
    p = subprocess.run(command,capture_output=True,text=True,timeout=30)
    scheduler[label] = dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
    # Preserve observation failure explicitly; published checker evidence is
    # independent. No terminal/success conclusion comes from absent accounting.
    if label == 'queue': assert p.returncode == 0,p.stderr
report = root/'report-to-andrew/20260911-0135-root-collection'
for name in ('report.tex','report.pdf','report.log'): bind(report/name)
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
commands = [[sys.executable,'-m','unittest','discover','-s',d,'-p','test_*.py'] for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable,'tools/docs/'+name+'.py'] for name in (
    'check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary',
    'check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name+'.py','--self-test'] for name in (
    'localize-reduce-envelope-7216','localize-reduce-collection-7217','validate-rank31-retry-producer-7211')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name+'.py'] for name in (
    'validate-vivify-revalidation-full50-7168','validate-factor-arena-full50-7201',
    'check-submit-congruence-fused-runtime-7205','validate-congruence-fused-profiles-7206',
    'check-submit-factor-arena-transfer-controls-7212','check-submit-root-binary-BC-7221')]
checks = {}
for command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode,output=p.stdout+p.stderr)
    assert p.returncode == 0,(command,p.stdout,p.stderr)
    print('PASS',' '.join(command),flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,owned_paths=sorted(owned),
        campaigns=campaigns,scheduler=scheduler,checks=checks,fixture_failures=failures,
        rank31_prephase_extra_marks=extra,rank31_reduce_boundaries=macro,congruence_profile_ratios=profiles,
        cleanup={'successful_fixed_proofs_bytes':proof['removed_bytes'],'verified_profile_expansions_bytes':2398041756,
                 'failed_remnants':'preserved; no unrelated deletion'},
        previous_turn='Read-only bookkeeping recheck reconfirmed known evidence; no progress toward implementation. This turn completed new full suites, sanitized/proof/model gates, submitted7221 and final7205/7206 waves, and advanced profile attribution.',
        decision='Retain7144; full50 now50checked-clean. Candidate7218 validated locally but no runtime gain or promotion claimed. Finish independent AND measurements; gate root-binary runtime on66exactBC solver outcomes without proof waits.',
        limitation='Scheduler observation errors preserved, not treated as terminal. No current-source fuzz acceptance, denied transfer/push retry, integration or goal completion. Memory/final other proofs and original convergence remain open.'),out,indent=2)
print(target)
print('Owned paths',len(owned))
for name,r in campaigns.items(): print(name,r['published'],'published',r['checked'],'checked')
