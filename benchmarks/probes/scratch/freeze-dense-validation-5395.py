#!/usr/bin/env python3
"""Retain final local validation and the mandatory compiled-report hashes."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
log = root / 'build/release/Testing/Temporary/LastTest.log'
body = log.read_text()
assert body.count('Test Passed.') == 77 and 'Test Failed.' not in body
target = root / 'benchmarks/reports/2026-09-07-dense-entry-main-ctest.log'
assert not target.exists()
shutil.copy2(log, target)
report = root / 'report-to-andrew/20260907-1813-dense-entry'
assert 'Overfull' not in (report / 'report.log').read_text()
assert (report / 'report.pdf').stat().st_size > 0
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              main_ctest=dict(passed=77, seconds=20.40, log=str(target.relative_to(root)), sha256=sha(target)),
              doc_size=dict(errors=0, budgets=30, near_cap_warnings=4, progress_lines=116),
              progress_check=dict(errors=0, warnings=['Three historical lesson citations in plan.', 'Old completed tier authority not cited in current resume surface.'],
                                  queue='Escalated read-only check observes the real active SLURM queue.'),
              option_coverage=dict(options=418, unaccounted=0, failures=0),
              section_refs=dict(resolved=42, errors=0),
              rule_policy_boundary='PASS',
              andrew_report=dict(directory=str(report.relative_to(root)), final_compilations=2,
                                 tex_sha256=sha(report / 'report.tex'), pdf_sha256=sha(report / 'report.pdf'),
                                 pages=3, overfull_boxes=0),
              scope='Fixed aggregate validation only; no fuzz campaign or fuzz artifact inspection. Candidate-specific full suites are independently retained in 5391.')
p = root / 'benchmarks/reports/2026-09-07-dense-entry-validation.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: main 77/77 and final TeX/PDF validation frozen')
