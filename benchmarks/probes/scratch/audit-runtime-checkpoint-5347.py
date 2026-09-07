#!/usr/bin/env python3
"""Scoped normal-campaign audit and explicit evidence staging list; no fuzz paths."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
names = [
    '5337-rank21-rewrite-source','5338-duplicate-fixture','5339-warmup-heap',
    '5340-warmup-implications','5341-warmup-reference','5342-deact-trace',
    '5343-round-retirement','5344-round-runtime','5345-retirement-fixture',
    '5346-round-frontier','5347-decision-stream']
campaigns = [ROOT/'benchmarks/runs'/('20260907-'+name) for name in names]
staged = [ROOT/p for p in ('PROGRESS.md','docs/implementation-notes.md',
    'docs/scheduling-parity-ledger.md','benchmarks/reports/2026-09-07-reference-duplicates-and-retirement.md')]
audit = dict(manifests={},checked_full_models=0,certificate_residue=[],local_only=[])
for c in campaigns:
    for p in c.glob('*manifest.json'):
        raw = p.read_bytes()
        m = json.loads(raw)
        for key in ('hashes','pin_sha256'):
            for rel,expected in m.get(key,{}).items():
                assert hashlib.sha256((c/rel).read_bytes()).hexdigest() == expected,(c,rel)
        audit['manifests'][str(p.relative_to(ROOT))] = hashlib.sha256(raw).hexdigest()
    for p in (c/'results').glob('*.json'):
        r = json.loads(p.read_text())
        assert r['valid'] and r['artifact_deleted'] and not r['cleanup_pending']
        assert r['model_check']['result']=='verified'
        audit['checked_full_models'] += 1
    for p in c.rglob('*'):
        if not p.is_file():
            continue
        rel = str(p.relative_to(ROOT))
        if p.suffix in ('.drat','.lrat','.lpr','.dpr','.cnf'):
            audit['certificate_residue'].append(rel)
        if p.suffix in ('.json','.py','.patch','.sbatch','.log','.out','.tsv') and p.stat().st_size < 2_000_000:
            staged.append(p)
        else:
            audit['local_only'].append(dict(path=rel,bytes=p.stat().st_size))
assert audit['checked_full_models']==17 and not audit['certificate_residue']
audit.update(manual_deletions=0,
    job_audit='All 31 submitted tasks terminal; stopped-run logs and checked results confirm scratch cleanup. squeue shows only user interactive bash allocation 1668242; no solver campaigns.',
    ownership='Only the eleven explicitly named normal campaigns inspected; no fuzz artifacts or campaigns independently audited.')
destination = campaigns[-1]/'artifact-audit.json'
if not destination.exists():
    destination.write_text(json.dumps(audit,indent=2)+'\n')
staged.append(destination)
for name in ('aggregate.py','summary.json','runtime-summary.tsv','results/035.json'):
    staged.append(ROOT/'benchmarks/runs/20260907-5314-pose-r2-ranks1-40'/name)
helpers = [
    'prepare-rank21-rewrite-source-5337.py','sweep-duplicate-reference-5338.c',
    'sweep-duplicate-tactix-5338.cpp','prepare-warmup-heap-5339.py',
    'prepare-warmup-implications-5340.py','prepare-warmup-reference-5341.py',
    'prepare-deact-trace-5342.py','prepare-round-retirement-5343.py',
    'record-runtime-checkpoint-5343.py','prepare-round-runtime-5344.py',
    'sweep-retirement-rounds-5345.cpp','replay-retirement-fixture-5345.py',
    'prepare-round-frontier-5346.py','seal-runtime-checkpoint-5346.py',
    'prepare-decision-stream-5347.py','audit-runtime-checkpoint-5347.py']
for name in helpers:
    p = ROOT/'benchmarks/probes/scratch'/name
    assert p.is_file(),p
    staged.append(p)
staged = sorted(set(staged))
if '--stage' in sys.argv:
    subprocess.run(['git','add','-f','--',*map(str,staged)],cwd=ROOT,check=True)
print(json.dumps(dict(checked_full_models=audit['checked_full_models'],certificate_residue=audit['certificate_residue'],
    explicit_staging_files=len(staged),local_only_files=len(audit['local_only']),
    manual_deletions=0),indent=2))
