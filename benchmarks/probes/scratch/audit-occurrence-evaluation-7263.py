#!/usr/bin/env python3
"""Freeze repeated cost/memory evidence and the independently owned full-set gate."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

target = root/'benchmarks/reports/2026-09-11-occurrence-evaluation-audit-7263.json'
assert not target.exists(), 'successful audit is frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    digest = sha(p); assert expected is None or digest == expected, p
    rel = str(p.relative_to(root)); hashes[rel] = digest
    if own: owned.add(rel)
def read(p, own=False):
    bind(p,own=own)
    return json.loads(p.read_text())

prior = read(root/'benchmarks/reports/2026-09-11-occurrence-cutover-audit-7258.json')
for rel in prior['owned_paths']:
    if rel not in ('PROGRESS.md','docs/implementation-notes.md'):
        bind(root/rel,prior['hashes'][rel])
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
for rel in ('PROGRESS.md','docs/implementation-notes.md','DEVELOPMENT_PLAN.md','CLAUDE.md'):
    bind(root/rel,own=rel == 'PROGRESS.md')
build = read(root/'benchmarks/runs/20260911-7253-occurrence-cutover-build/manifest.json')
for rel,digest in build['files'].items(): bind(root/build['source']/rel,digest)
for rel,digest in build['binaries'].items(): bind(root/rel,digest)
assert build['status'] == 'PASS'
for stem in ('prepare-occurrence-profiles-7259','validate-occurrence-profiles-7259',
    'submit-occurrence-profile-wave-7259','extract-occurrence-wave-7260',
    'assess-occurrence-7261','prepare-occurrence-full50-7262',
    'submit-occurrence-full50-7262','validate-occurrence-full50-7262',
    'decompose-occurrence-cost-7264','audit-occurrence-evaluation-7263'):
    bind(root/f'benchmarks/probes/scratch/{stem}.py',own=True)
assessment = read(root/'benchmarks/reports/2026-09-11-occurrence-assessment-7261.json',own=True)
decomposition = read(root/'benchmarks/reports/2026-09-11-occurrence-cost-decomposition-7264.json',own=True)
for result in (assessment,decomposition):
    for rel,digest in result['hashes'].items(): bind(root/rel,digest)
assert assessment['runtime_expected'] == 18 and assessment['profile_expected'] == 6
assert assessment['complete_eliminate']['max'] < 1
assert len(decomposition['waves']) == 3
deleted = 0
for wave in range(3):
    directory = root/f'benchmarks/runs/20260911-7260-occurrence-samples/wave{wave}'
    summary = read(directory/'summary.json')
    assert not list(directory.glob('expanded-*'))
    deleted += summary['temporary_expansions_deleted_bytes']
    for p in directory.iterdir():
        assert p.is_file()
        bind(p,own=True)
bind(root/'benchmarks/reports/2026-09-11-occurrence-evaluation-7263.md',own=True)

expected = {name:value['expected'] for name,value in prior['campaigns'].items()}
expected.update({'20260911-7259-occurrence-profiles':6,'20260911-7262-occurrence-ranks1-50':50})
new_names = {'20260911-7257-occurrence-runtime','20260911-7259-occurrence-profiles','20260911-7262-occurrence-ranks1-50'}
campaigns, jobs = {}, set()
for name,count in expected.items():
    c = root/'benchmarks/runs'/name
    cm = read(c/'manifest.json',own=name in new_names)
    for rel,digest in cm['pin_sha256'].items():
        bind(c/rel,digest,own=name in new_names and (c/rel).suffix in ('.py','.tsv','.sbatch','.patch'))
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p,own=name in new_names)
            assert launch['manifest_sha256'] == sha(c/'manifest.json')
            jobs.add(str(launch['job_id']))
    states = {}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r = read(p,own=True); task = r['task']
            assert task == int(p.stem) and 0 <= task < count
            state = classify(r,folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            states[task] = state
    campaigns[name] = dict(expected=count,published=len(states),checked=sum(v['checked'] for v in states.values()),records=states)
assert campaigns['20260910-7167-vivify-revalidation-ranks1-50']['checked'] == 50
assert campaigns['20260911-7257-occurrence-runtime']['published'] == 18
assert campaigns['20260911-7259-occurrence-profiles']['published'] == 6
for p in (root/'benchmarks/runs/20260911-7259-occurrence-profiles/profiles').iterdir():
    if p.is_file(): bind(p)  # Frozen raw profiles/reports remain local, hash-bound.

scheduler = {}
for label,command in (
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),
                   '-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])):
    p = subprocess.run(command,capture_output=True,text=True,timeout=30)
    scheduler[label] = dict(command=command,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr)
    if label == 'queue': assert p.returncode == 0,p.stderr

report = root/'report-to-andrew/20260911-0425-occurrence-evaluation'
for name in ('report.tex','report.pdf','report.log'): bind(report/name)
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
commands = [[sys.executable,'-m','unittest','discover','-s',d,'-p','test_*.py'] for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable,f'tools/docs/{name}.py'] for name in (
    'check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary',
    'check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,f'benchmarks/probes/scratch/{name}.py','--self-test'] for name in (
    'check-submit-occurrence-runtime-7257','assess-occurrence-7261',
    'validate-occurrence-full50-7262','decompose-occurrence-cost-7264')]
commands += [[sys.executable,f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-vivify-revalidation-full50-7168','validate-factor-arena-full50-7201',
    'check-submit-congruence-fused-runtime-7205','validate-congruence-fused-profiles-7206',
    'check-submit-root-binary-runtime-7226','validate-root-binary-profiles-7230',
    'check-submit-gc-forward-runtime-7240','validate-gc-forward-profiles-7241',
    'check-submit-occurrence-BC-7256','check-submit-occurrence-runtime-7257',
    'validate-occurrence-profiles-7259','validate-occurrence-full50-7262')]
checks = {}
for command in commands:
    p = subprocess.run(command,cwd=root,capture_output=True,text=True)
    checks[' '.join(command)] = dict(exit_code=p.returncode,output=p.stdout+p.stderr)
    assert p.returncode == 0,(command,p.stdout,p.stderr)
    print('PASS',' '.join(command),flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,
        owned_paths=sorted(owned),campaigns=campaigns,scheduler=scheduler,checks=checks,
        previous_turn='No convergence progress: the user-requested read-only bookkeeping audit confirmed existing evidence. This turn executes remaining runtime/profile waves, explains complete-phase costs and launches the separately pinned memory-priority full50 gate.',
        decision='No proven net runtime gain or promotion. The repeated single-image memory reduction and whole-Eliminate improvement justify measuring the complete memory/runtime tradeoff on all50. Retained7144 and productionc2a95a6 unchanged.',
        newly_deleted_bytes=deleted,
        cleanup='Only7260 verified temporary perf expansions removed. Original compressed profiles and extracted samples retained. No existing campaign, source, active checker scratch or successful writer altered.',
        non_solver_errors='One earlier incorrect unittest module invocation was corrected to discovery (15 callstack and6 envelope tests passed). One interactive oversized JSON inspection was truncated and then narrowed; neither was solver evidence or a changed gate.',
        limitation='Original runtime target, memory parity, current-source Claude-owned fuzz, remaining independent checks, zero residue and consolidation remain open. No denied fuzz access/transfer or push retry.'),out,indent=2)
print(target)
