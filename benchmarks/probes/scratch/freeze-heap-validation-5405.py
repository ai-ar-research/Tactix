#!/usr/bin/env python3
"""Freeze completed fixed validation and the twice-compiled Andrew report."""
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
target = root / 'benchmarks/reports/2026-09-07-heap-localization-main-ctest.log'
assert not target.exists()
shutil.copy2(log, target)
report = root / 'report-to-andrew/20260907-1905-heap-localization'
assert 'Overfull' not in (report / 'report.log').read_text()
for n in (1, 2): assert 'Output written on report.pdf' in (report / f'compile-{n}.log').read_text()
assert (report / 'report.pdf').stat().st_size > 0
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              main_ctest=dict(passed=77, seconds=5.07, log=str(target.relative_to(root)), sha256=sha(target)),
              diagnostic_ctests={'5399': dict(passed=76, seconds=21.22), '5402': dict(passed=76, seconds=6.29)},
              doc_size=dict(errors=0, budgets=30, near_cap_warnings=4, progress_lines=124),
              progress_check=dict(errors=0, warnings=['Hardware L3 terms misidentified as three lesson references.', 'Historical August-13 completed tier not cited as current authority.'], slurm_visible=True),
              option_coverage=dict(options=418, unaccounted=0, failures=0),
              section_refs=dict(resolved=42, errors=0), rule_policy_boundary='PASS',
              andrew_report=dict(directory=str(report.relative_to(root)), final_compilations=2, pages=3, overfull_boxes=0,
                                 tex_sha256=sha(report / 'report.tex'), pdf_sha256=sha(report / 'report.pdf')),
              limitations='Only fixed tests and scoped normal diagnostics; no new fuzz campaign or fuzz artifact inspection. No production change or new runtime-acceptance claim.')
p = root / 'benchmarks/reports/2026-09-07-heap-localization-validation.json'
assert not p.exists()
p.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: fixed tests, document checks and compiled report validation frozen')
