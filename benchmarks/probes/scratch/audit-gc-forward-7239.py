#!/usr/bin/env python3
"""Freeze the fused collector gate and matched root-discovery measurements."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-gc-forward-audit-7239.json'
assert not target.exists(), 'successful checkpoint frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    digest = sha(p); assert expected is None or digest == expected, p
    rel = str(p.relative_to(root)); hashes[rel] = digest
    if own: owned.add(rel)
def bound_json(p, own=False):
    r = json.loads(p.read_text()); bind(p, own=own)
    for rel, digest in r.get('hashes', {}).items(): bind(root / rel, digest)
    return r
old_path = root / 'benchmarks/reports/2026-09-11-and-collection-audit-7232.json'
old = json.loads(old_path.read_text()); bind(old_path)
for rel in old['owned_paths']:
    if rel != 'PROGRESS.md': bind(root / rel, old['hashes'][rel])
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'], cwd=root)
for rel in ('PROGRESS.md', 'docs/implementation-notes.md', 'CLAUDE.md', 'DEVELOPMENT_PLAN.md'):
    bind(root / rel, own=rel in ('PROGRESS.md', 'docs/implementation-notes.md'))
helpers = ('build-gc-forward-7233','check-gc-forward-sanitizers-7234',
           'check-gc-forward-proofs-7235','localize-root-binary-collection-7236',
           'check-gc-forward-root-lock-7237','prepare-gc-forward-BC-7238',
           'check-submit-gc-forward-BC-7238','audit-gc-forward-7239')
for name in helpers: bind(root / f'benchmarks/probes/scratch/{name}.py', own=True)
build = root / 'benchmarks/runs/20260911-7233-gc-forward-build'
b = json.loads((build / 'manifest.json').read_text())
for rel, digest in b['files'].items(): bind(root / b['source'] / rel, digest)
for rel, digest in b['binaries'].items(): bind(root / rel, digest)
assert b['binaries']['build/pin-7233-gc-forward-release/tactix'] == b['binaries']['build/pin-7233-gc-forward-bc/tactix']
for stem in ('7233-gc-forward-build','7234-gc-forward-sanitizers',
             '7235-gc-forward-proofs','7237-gc-forward-root-lock'):
    directory = root / ('benchmarks/runs/20260911-' + stem)
    for p in sorted(directory.rglob('*')):
        if p.is_file() and p.suffix in ('.json','.log','.patch'): bind(p, own=True)
    if stem.startswith('7233'): continue
    g = bound_json(directory / 'summary.json')
    assert g['status'] == 'PASS'
    assert not list(directory.rglob('*.cnf')) and not list(directory.rglob('*.proof'))
proofs = json.loads((root / 'benchmarks/runs/20260911-7235-gc-forward-proofs/summary.json').read_text())
locks = json.loads((root / 'benchmarks/runs/20260911-7237-gc-forward-root-lock/summary.json').read_text())
assert proofs['checked_UNSAT'] == 216 and proofs['validated_SAT_models'] == 216
assert locks['checked'] == 16
assert proofs['negative_controls'] == locks['negative_controls'] == 2
assert proofs['exact_parent_certificates'] and locks['exact_parent_certificates']
for kind, count in (('release',117),('bc',117),('trace',116)):
    assert f'100% tests passed out of {count}' in (build / f'logs/{kind}-ctest.log').read_text()
    assert (build / f'logs/{kind}-test_gc_forward.log').read_bytes() == (build / 'logs/previous-test_gc_forward.log').read_bytes()
deleted = proofs['removed_bytes'] + locks['removed_bytes']
for wave in range(3):
    p = root / f'benchmarks/runs/20260911-7231-root-binary-samples/wave{wave}/summary.json'
    if not p.exists(): continue
    r = bound_json(p, own=True); deleted += r['temporary_expansions_deleted_bytes']
    assert not list(p.parent.glob('expanded-*'))
    q = root / f'benchmarks/reports/2026-09-11-root-binary-collection-7236-wave{wave}.json'
    if q.exists(): bound_json(q, own=True)
bind(root / 'benchmarks/reports/2026-09-11-gc-forward-checkpoint-7239.md', own=True)
specs = [('20260910-7167-vivify-revalidation-ranks1-50',50),
         ('20260911-7201-factor-arena-ranks1-50',50),
         ('20260911-7202-congruence-fused-BC',66),
         ('20260911-7205-congruence-fused-runtime',18),
         ('20260911-7206-congruence-fused-profiles',6),
         ('20260911-7210-rank31-retry-producer',2),
         ('20260911-7221-root-binary-BC',66),
         ('20260911-7226-root-binary-runtime',18),
         ('20260911-7230-root-binary-profiles',6),
         ('20260911-7238-gc-forward-BC',66)]
campaigns, jobs = {}, set()
for name, count in specs:
    c = root / 'benchmarks/runs' / name
    m = json.loads((c / 'manifest.json').read_text()); bind(c / 'manifest.json', own=True)
    for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
    for group in ('dependencies','protected_campaign_hashes'):
        for rel, digest in m.get(group,{}).items(): bind(root / rel, digest)
    for p in c.iterdir():
        if p.is_file() and p.suffix in ('.json','.tsv','.sbatch'):
            bind(p, own=True)
            if p.name == 'launch.json' or p.name.endswith('-submission.json'):
                launch = json.loads(p.read_text())
                assert launch['manifest_sha256'] == sha(c / 'manifest.json')
                jobs.add(str(launch['job_id']))
    for p in (c / 'pins').iterdir():
        if p.suffix in ('.py','.patch'): bind(p, own=True)
    rows = {}
    for folder in ('provisional','results'):
        for p in sorted((c / folder).glob('*.json')):
            r = json.loads(p.read_text()); task = r['task']
            assert 0 <= task < count and task == int(p.stem)
            state = classify(r, folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'), (p,state)
            bind(p, own=True); rows[task] = state
    campaigns[name] = dict(expected=count,published=len(rows),checked=sum(r['checked'] for r in rows.values()),records=rows)
assert campaigns[specs[0][0]]['checked'] == 50
scheduler = {}
for name, command in (
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),'-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])):
    p = subprocess.run(command,capture_output=True,text=True,timeout=30)
    scheduler[name] = dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
    if name == 'queue': assert p.returncode == 0,p.stderr
report = root / 'report-to-andrew/20260911-0214-gc-forward'
for name in ('report.tex','report.pdf','report.log'): bind(report / name)
assert (report / 'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report / 'report.log').read_text()
commands = [[sys.executable,'-m','unittest','discover','-s',d,'-p','test_*.py'] for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable,'tools/docs/'+name+'.py'] for name in (
    'check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary',
    'check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name+'.py','--self-test'] for name in (
    'localize-root-binary-collection-7236','check-submit-root-binary-runtime-7226')]
commands += [[sys.executable,'benchmarks/probes/scratch/'+name+'.py'] for name in (
    'validate-vivify-revalidation-full50-7168','validate-factor-arena-full50-7201',
    'check-submit-congruence-fused-runtime-7205','validate-congruence-fused-profiles-7206',
    'check-submit-root-binary-BC-7221','check-submit-root-binary-runtime-7226',
    'validate-root-binary-profiles-7230','check-submit-gc-forward-BC-7238')]
checks = {}
for command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode,output=p.stdout+p.stderr)
    assert p.returncode == 0,(command,p.stdout,p.stderr)
    print('PASS',' '.join(command),flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,owned_paths=sorted(owned),
        campaigns=campaigns,scheduler=scheduler,checks=checks,
        previous_turn='Read-only bookkeeping recheck confirmed prior evidence without changing next action; revalidated here and resumed concrete implementation rather than another status-only turn.',
        decision='7233 fused sparse filter/forward is independently based on retained7144.117/117/116 suites,26240 watch comparisons,896 actual parent collector states,12 sanitizer runs and232 checked fixed UNSAT proofs pass.7238 BC launched; no performance claim or promotion. Continue predeclared root-binary waves without proof wait.',
        newly_deleted_bytes=deleted,cleanup='Only hash-verified owned profile expansions and consumed fixed micro-test inputs/proofs removed. Compressed profiles, extracted streams, active campaign scratch and prior failed fixtures preserved.',
        limitation='Production and submitted old pins unchanged. Source fuzz inaccessible under existing denial, no inspection/transfer/push retry. Goal, memory parity, broad current checks and consolidation remain open.'),out,indent=2)
print(target)
for name,r in campaigns.items(): print(name,r['published'],r['checked'])
