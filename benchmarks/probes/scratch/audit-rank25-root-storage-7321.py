#!/usr/bin/env python3
"""Freeze the root-storage lead and explicit7319 heap-attribution correction."""
import datetime as dt
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
target = root / 'benchmarks/reports/2026-09-11-rank25-root-storage-audit-7321.json'
assert not target.exists(), 'successful audit frozen'
hashes, owned = {}, set()
def bind(p, expected=None, own=False):
    rel = str(p.relative_to(root)); digest = sha(p)
    assert expected is None or expected == digest, p
    hashes[rel] = digest
    if own: owned.add(rel)
prior_path = root / 'benchmarks/reports/2026-09-11-full-clause-memory-audit-7319.json'
bind(prior_path)
prior = json.loads(prior_path.read_text())
for rel, digest in prior['hashes'].items():
    if rel != 'PROGRESS.md': bind(root / rel, digest)
for rel in ('PROGRESS.md', 'benchmarks/probes/scratch/rank25-layout-7320.cpp',
            'benchmarks/probes/scratch/analyze-rank25-root-storage-7320.py',
            'benchmarks/probes/scratch/audit-rank25-root-storage-7321.py',
            'benchmarks/reports/2026-09-11-rank25-root-storage-7320.json',
            'benchmarks/reports/2026-09-11-rank25-root-storage-7321.md'):
    bind(root / rel, own=True)
analysis = json.loads((root / 'benchmarks/reports/2026-09-11-rank25-root-storage-7320.json').read_text())
for rel,digest in analysis['hashes'].items(): bind(root / rel, digest)
assert not subprocess.check_output(['git','diff','c2a95a6','--','src','include'],cwd=root)
campaigns, jobs = {}, set()
for name, old in prior['campaigns'].items():
    c = root / 'benchmarks/runs' / name
    states = {}
    for folder in ('provisional','results'):
        for p in sorted((c/folder).glob('*.json')):
            r = json.loads(p.read_text()); bind(p,own=True)
            task = r['task']; assert task == int(p.stem) and 0 <= task < old['expected']
            state = classify(r,folder == 'results')
            assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT'),(p,state)
            states[task] = state
    campaigns[name] = dict(expected=old['expected'],published=len(states),
                          checked=sum(s['checked'] for s in states.values()),records=states)
    for p in c.iterdir():
        if p.name == 'launch.json' or p.name.endswith('-submission.json'):
            jobs.add(str(json.loads(p.read_text())['job_id']))
scheduler = {}
for name,command in (
    ('queue',['squeue','-h','-r','-j',','.join(sorted(jobs)),'-o','%i|%T|%j']),
    ('accounting',['sacct','-n','-X','--array','-j',','.join(sorted(jobs)),
                  '-S','2026-09-10T21:26:00','-o','JobID,State%40,ExitCode,Start','-P'])):
    p = subprocess.run(command,text=True,capture_output=True,timeout=30)
    assert p.returncode == 0,(command,p.stderr)
    scheduler[name] = dict(command=command,stdout=p.stdout,stderr=p.stderr)
report = root / 'report-to-andrew/20260911-0835-rank25-storage'
for name in ('report.tex','report.pdf','report.log'):bind(report/name)
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
assert 'Overfull' not in (report/'report.log').read_text()
commands = [[sys.executable,'-m','unittest','discover','-s',d,'-p','test_*.py'] for d in ('tools/bench','tools/slurm')]
commands += [[sys.executable,f'tools/docs/{name}.py'] for name in (
    'check-doc-size','check-progress','check-section-refs','check-rule-policy-boundary',
    'check-default-strategy-config','check-proof-rule-sync','check-option-coverage')]
commands += [[sys.executable,f'benchmarks/probes/scratch/{name}.py'] for name in (
    'analyze-rank25-root-storage-7320','validate-rank25-memory-7315','validate-clause-full50-7312')]
checks = {}
for command in commands:
    p = subprocess.run(command,cwd=root,text=True,capture_output=True)
    assert p.returncode == 0,(command,p.stdout,p.stderr)
    checks[' '.join(command)] = dict(exit_code=p.returncode,output=p.stdout+p.stderr)
    print('PASS',' '.join(command),flush=True)
with target.open('x') as out:
    json.dump(dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(),hashes=hashes,
        owned_paths=sorted(owned),campaigns=campaigns,scheduler=scheduler,checks=checks,
        previous_turn='Progress: checked Rank25 exact-Release peak localization and complete full50 table committed23d2f4d7. This increment corrects one exploratory heap attribution and identifies physical root-fixed storage as a major structural lead.',
        decision=analysis['decision'],correction=analysis['correction'],newly_deleted_bytes=0,
        limitation='No source promotion or speedup. Pending independent proofs continue. Preserve raw/gzip exceptions; no denied fuzz access/transfer or push retry. Goal, current-source fuzz and consolidation unfinished.'),out,indent=2)
print(target)
