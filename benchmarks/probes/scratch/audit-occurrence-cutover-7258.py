#!/usr/bin/env python3
"""Freeze verified consumer migration, correctness gates and owned live jobs."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-occurrence-cutover-audit-7258.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    digest = sha(p)
    assert expected is None or digest == expected, p
    rel = str(p.relative_to(root)); hashes[rel] = digest
    if own: owned.add(rel)
def read(p, own=False, dependencies=False):
    bind(p, own=own)
    value = json.loads(p.read_text())
    if dependencies:
        for rel,digest in value.get('hashes', {}).items(): bind(root / rel, digest)
    return value
prior = read(root / 'benchmarks/reports/2026-09-11-single-occurrence-audit-7252.json')
for rel in prior['owned_paths']:
    if rel not in ('PROGRESS.md','docs/implementation-notes.md'):
        bind(root / rel, prior['hashes'][rel])
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'], cwd=root)
for rel in ('PROGRESS.md','docs/implementation-notes.md','DEVELOPMENT_PLAN.md','CLAUDE.md'):
    bind(root / rel, own=rel in ('PROGRESS.md','docs/implementation-notes.md'))
for name in ('build-occurrence-cutover-7253.py', 'check-occurrence-cutover-sanitizers-7254.py',
    'test-occurrence-proof-7255.cpp','check-occurrence-proofs-7255.py',
    'occurrence-fingerprint-7256.py','prepare-occurrence-BC-7256.py',
    'check-submit-occurrence-BC-7256.py','prepare-occurrence-runtime-7257.py',
    'check-submit-occurrence-runtime-7257.py','audit-occurrence-cutover-7258.py'):
    bind(root / 'benchmarks/probes/scratch' / name, own=True)
build = root / 'benchmarks/runs/20260911-7253-occurrence-cutover-build'
m = read(build / 'manifest.json', own=True)
assert m['status'] == 'PASS'
for rel,digest in m['files'].items(): bind(root / m['source'] / rel, digest)
for group in ('binaries','parent_objects'):
    for rel,digest in m[group].items(): bind(root / rel,digest)
for kind,count in (('release',117),('bc',117),('trace',116)):
    p = build / f'logs/{kind}-ctest.log'
    content = p.read_text()
    assert f'100% tests passed out of {count}' in content
    assert 'Skipped' not in content and 'Not Run' not in content
assert '52 boundaries=364 state=6da9d99c105a60e1' in m['oracle']
assert '840 state=f86da0be45055db3' in m['replacement']
san_dir = root / 'benchmarks/runs/20260911-7254-occurrence-cutover-sanitizers'
proof_dir = root / 'benchmarks/runs/20260911-7255-occurrence-proofs'
san = read(san_dir / 'summary.json', dependencies=True)
assert san['status'] == 'PASS' and len(san['outcomes']) == 8
assert all(len(outcomes) == 2 for outcomes in san['outcomes'].values())
proof = read(proof_dir / 'summary.json', dependencies=True)
assert proof['status'] == 'PASS' and proof['checked_UNSAT'] == proof['validated_SAT_models'] == 156
assert proof['negative_controls'] == 2 and proof['exact_parent_certificates']
assert not list(proof_dir.glob('*.cnf')) and not list(proof_dir.glob('*.proof'))
for directory in (build,san_dir,proof_dir):
    for p in directory.rglob('*'):
        if p.is_file() and p.suffix in ('.json','.log','.patch'): bind(p,own=True)
bind(root / 'benchmarks/reports/2026-09-11-occurrence-cutover-checkpoint-7258.md',own=True)
expected = {name:old['expected'] for name,old in prior['campaigns'].items()}
expected['20260911-7256-occurrence-BC'] = 66
runtime = root / 'benchmarks/runs/20260911-7257-occurrence-runtime'
if (runtime / 'manifest.json').exists(): expected[runtime.name] = 18
campaigns, jobs = {}, set()
for name,count in expected.items():
    c = root / 'benchmarks/runs' / name
    new = name in ('20260911-7256-occurrence-BC','20260911-7257-occurrence-runtime')
    cm = read(c / 'manifest.json',own=new)
    for rel,digest in cm['pin_sha256'].items():
        bind(c / rel,digest,own=new and (c / rel).suffix in ('.py','.tsv','.sbatch','.patch'))
    for p in c.iterdir():
        if p.is_file() and (p.name == 'launch.json' or p.name.endswith('-submission.json')):
            launch = read(p,own=new)
            assert launch['manifest_sha256'] == sha(c / 'manifest.json')
            jobs.add(str(launch['job_id']))
    rows = {}
    for folder in ('provisional','results'):
        for p in sorted((c / folder).glob('*.json')):
            r = read(p,own=True); task = r['task']
            assert task == int(p.stem) and 0 <= task < count
            state = classify(r,folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'), (p,state)
            rows[task] = state
    campaigns[name] = dict(expected=count,published=len(rows),
        checked=sum(row['checked'] for row in rows.values()),records=rows)
assert campaigns['20260910-7167-vivify-revalidation-ranks1-50']['checked'] == 50
scheduler = {}
for name,command in (
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),
                   '-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])):
    r = subprocess.run(command,capture_output=True,text=True,timeout=30)
    scheduler[name] = dict(command=command,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr)
    if name == 'queue': assert r.returncode == 0,r.stderr
report = root / 'report-to-andrew/20260911-0348-occurrence-cutover'
for name in ('report.tex','report.pdf','report.log'): bind(report / name)
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
commands = [[sys.executable,'-m','unittest','discover','-s',d,'-p','test_*.py']
            for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable,f'tools/docs/{name}.py'] for name in (
    'check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary',
    'check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,'benchmarks/probes/scratch/occurrence-fingerprint-7256.py'],
             [sys.executable,'benchmarks/probes/scratch/check-submit-occurrence-runtime-7257.py','--self-test']]
commands += [[sys.executable,f'benchmarks/probes/scratch/{name}.py'] for name in (
    'validate-vivify-revalidation-full50-7168','validate-factor-arena-full50-7201',
    'check-submit-congruence-fused-runtime-7205','validate-congruence-fused-profiles-7206',
    'check-submit-root-binary-runtime-7226','validate-root-binary-profiles-7230',
    'check-submit-gc-forward-runtime-7240','validate-gc-forward-profiles-7241',
    'check-submit-occurrence-BC-7256')]
if (runtime / 'manifest.json').exists():
    commands.append([sys.executable,'benchmarks/probes/scratch/check-submit-occurrence-runtime-7257.py'])
checks = {}
for command in commands:
    r = subprocess.run(command,cwd=root,capture_output=True,text=True)
    checks[' '.join(command)] = dict(exit_code=r.returncode,output=r.stdout+r.stderr)
    assert r.returncode == 0,(command,r.stdout,r.stderr)
    print('PASS',' '.join(command),flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,
        owned_paths=sorted(owned),campaigns=campaigns,scheduler=scheduler,checks=checks,
        previous_turn='No goal progress: read-only bookkeeping recheck confirmed existing evidence. Revalidated handoff and advanced actual consumer migration this turn.',
        decision='7253 single-image consumer cutover passes actual-parent order/state, full suites, independent proof/model and sanitizer gates;7256 BC launched. Runtime/RSS and complete Eliminate remain unproven; retained7144 baseline, productionc2a95a6 unchanged.',
        newly_deleted_bytes=proof['removed_bytes'],
        cleanup='Only63972 bytes of generated fixed proof/model CNFs and certificates removed by7255; all check commands/hashes retained. No old campaign, live scratch or profile artifact removed.',
        limitation='Original target, memory parity, current-source Claude-owned fuzz, checked campaign completion and consolidation remain open. No denied fuzz access/transfer or push retry.'),out,indent=2)
print(target)
