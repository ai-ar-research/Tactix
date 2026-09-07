#!/usr/bin/env python3
"""Freeze scoped provenance and validation without waiting on proof jobs."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
jobs = {5383: 1669005, 5389: 1669095, 5393: 1669174, 5395: 1669252,
        5406: 1669559, 5407: None, 5408: 1669577, 5410: 1669611}
campaigns = []
for number, job in jobs.items():
    c = next((root / 'benchmarks/runs').glob(f'20260907-{number}-*'))
    m = json.loads((c / 'manifest.json').read_text())
    for rel, digest in m.get('hashes', m.get('pin_sha256', {})).items():
        assert sha(c / rel) == digest, (number, rel)
    records = sorted((c / 'results').glob('*.json'))
    for p in records:
        r = json.loads(p.read_text())
        if number >= 5406:
            assert r['exit_code'] == 0 and r['scratch_deleted']
            for name, digest in r['trace_sha256'].items(): assert sha(c / 'traces' / name) == digest
        else: assert r['valid'] and r['artifact_deleted'] and not r['cleanup_pending']
    residue = [str(p.relative_to(root)) for p in c.rglob('*') if p.is_file() and p.suffix in ('.cnf', '.drat', '.lrat', '.frat', '.grat')]
    assert not residue, residue
    state = subprocess.run(['sacct', '-j', str(job), '-X', '--format=JobID,State,ExitCode', '--parsable2'], capture_output=True, text=True, check=True).stdout.splitlines() if job else None
    campaigns.append(dict(campaign=str(c.relative_to(root)), job=job, slurm_state=state,
                          manifest_sha256=sha(c / 'manifest.json'), final_records=len(records),
                          result_sha256={p.name: sha(p) for p in records},
                          summary_sha256={p.name: sha(p) for p in c.glob('*summary.json')},
                          repository_residue=residue,
                          retention='Active checker or unique diagnostic/current authority; preserve all submitted pins, runners, raw local traces and dependencies.'))
checks = {}
for name in ('check-doc-size', 'check-progress', 'check-section-refs', 'check-option-coverage', 'check-rule-policy-boundary'):
    p = subprocess.run(['python3', str(root / 'tools/docs' / (name + '.py'))], cwd=root, capture_output=True, text=True, check=True)
    checks[name] = p.stdout + p.stderr
report = root / 'report-to-andrew/20260907-1930-async-warmup'
assert 'Overfull' not in (report / 'report.log').read_text()
for n in (1, 2): assert 'Output written on report.pdf' in (report / f'compile-{n}.log').read_text()
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), campaigns=campaigns,
              checks=checks, progress_lines=len((root / 'PROGRESS.md').read_text().splitlines()),
              prior_fixed_test_authority='benchmarks/reports/2026-09-07-heap-localization-validation.json',
              unchanged_solver_source=True, removed_bytes=0, uncertain_deletions=[],
              andrew_report=dict(directory=str(report.relative_to(root)), final_compilations=2,
                                 tex_sha256=sha(report / 'report.tex'), pdf_sha256=sha(report / 'report.pdf')),
              scope='Normal runtime and stopped diagnostics only; no fuzz campaign/artifact inspected. Active task scratch remains job-owned. Prior 5401 cleanup is not repeated.')
p = root / 'benchmarks/reports/2026-09-07-async-check-validation.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: asynchronous checkpoint provenance, cleanup and document/report validation frozen')
