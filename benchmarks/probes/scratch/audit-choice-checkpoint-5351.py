#!/usr/bin/env python3
"""Audit only 5348--5351 normal campaigns and stage their explicit evidence."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
names = ('20260907-5348-choice-state', '20260907-5349-choice-deactivation',
         '20260907-5350-bve-frontier', '20260907-5351-bve-schedule')
campaigns = [ROOT / 'benchmarks/runs' / name for name in names]
staged = [ROOT / 'PROGRESS.md', ROOT / 'benchmarks/reports/2026-09-07-bve-choice-frontier.md']
audit = dict(manifests={}, comparators={}, certificate_residue=[], local_only=[])
for c in campaigns:
    manifest = json.loads((c / 'manifest.json').read_text())
    for rel, expected in manifest['hashes'].items():
        assert hashlib.sha256((c / rel).read_bytes()).hexdigest() == expected, (c, rel)
    audit['manifests'][str(c.relative_to(ROOT))] = hashlib.sha256((c / 'manifest.json').read_bytes()).hexdigest()
    result = subprocess.run([sys.executable, str(c / 'compare.py')],
                            check=True, capture_output=True, text=True)
    assert json.loads(result.stdout) == json.loads((c / 'summary.json').read_text())
    audit['comparators'][str(c.relative_to(ROOT))] = 'summary reproduced'
    for p in c.rglob('*'):
        if not p.is_file():
            continue
        rel = str(p.relative_to(ROOT))
        if p.suffix in ('.drat', '.lrat', '.lpr', '.dpr', '.cnf'):
            audit['certificate_residue'].append(rel)
        if p.suffix in ('.json', '.py', '.patch', '.sbatch', '.log', '.out', '.tsv') and p.stat().st_size < 2_000_000:
            staged.append(p)
        else:
            audit['local_only'].append(dict(path=rel, bytes=p.stat().st_size,
                sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
assert not audit['certificate_residue']
audit.update(stopped_clean=8, checked_full_verdicts=0, manual_deletions=0,
    terminal_jobs=['1668712', '1668714', '1668716', '1668718'],
    job_audit='sacct: all eight tasks COMPLETED 0:0; runner logs confirm intentional zero stops and scratch_deleted True. squeue: only user interactive bash 1668242.',
    ownership='Only four explicitly named normal campaigns inspected; no independent fuzz-artifact audit.',
    preservation='Every campaign has unique causal evidence and retained parent references. No abandoned scaffold or certificate residue warrants deletion.')
destination = campaigns[-1] / 'artifact-audit.json'
if not destination.exists():
    destination.write_text(json.dumps(audit, indent=2) + '\n')
staged.append(destination)
for name in ('prepare-choice-state-5348.py', 'prepare-choice-deactivation-5349.py',
             'prepare-bve-frontier-5350.py', 'prepare-bve-schedule-5351.py',
             'seal-choice-summaries-5350.py', 'seal-bve-schedule-5351.py',
             'audit-choice-checkpoint-5351.py', 'validate-choice-checkpoint-5351.py'):
    path = ROOT / 'benchmarks/probes/scratch' / name
    assert path.is_file(), path
    staged.append(path)
staged = sorted(set(staged))
if '--stage' in sys.argv:
    subprocess.run(['git', 'add', '-f', '--', *map(str, staged)], cwd=ROOT, check=True)
print(json.dumps(dict(stopped_clean=8, certificate_residue=[], manual_deletions=0,
    explicit_staging_files=len(staged), local_only_files=len(audit['local_only'])), indent=2))
