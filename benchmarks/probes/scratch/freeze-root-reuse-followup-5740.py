#!/usr/bin/env python3
"""Bind late normal results and the public-target follow-up without changing pins."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
paths = [
    'benchmarks/reports/2026-09-08-root-conflict-reuse-validation.json',
    'benchmarks/reports/2026-09-08-root-reuse-followup.md',
    'benchmarks/runs/20260908-5735-root-reuse-rank31-prefix/summary.json',
    'benchmarks/runs/20260908-5679-assignment-level-rank39-runtime/summary.json',
    'benchmarks/runs/20260908-5718-root-conflict-reuse/manifest.json',
    '.claude/worktrees/runtime-root-conflict-reuse-5718/src/rules/analyze/analyze.cpp',
    '.claude/worktrees/runtime-root-conflict-reuse-5718/tests/test_root_conflict_reuse.cpp',
    'PROGRESS.md', 'docs/calculus.md', 'docs/implementation-notes.md',
    'report-to-andrew/20260908-0955-root-reuse-followup/report.tex',
    'report-to-andrew/20260908-0955-root-reuse-followup/report.pdf']
hashes = {p:sha(root / p) for p in paths}
prior = json.loads((root / paths[0]).read_text())
prefix = json.loads((root / paths[2]).read_text())
timing = json.loads((root / paths[3]).read_text())
assert prefix['clean_stops'] == 2 and prefix['stop_conflict'] == 300000
assert prefix['matching_rows'] == 76952511
assert prefix['first_decision_difference']['candidate'] == ['179823','76834806','41','-39842','0']
assert prefix['first_decision_difference']['kissat'] == ['179823','76834806','41','-32092','0']
assert timing['checked_clean'] == timing['solver_results'] == 6
assert timing['same_non_timing_statistics_and_proof'] and not timing['absolute_range_within_two_percent']
for item in prior['campaigns']:
    if not any(tag in item['campaign'] for tag in ('-5735-', '-5679-')): continue
    for name, digest in item['result_sha256'].items():
        assert sha(root / item['campaign'] / 'results' / name) == digest
manifest = json.loads((root / paths[4]).read_text())
for rel, digest in manifest['files'].items(): assert sha(root / manifest['source'] / rel) == digest
checks = {}
for name in ('check-doc-size', 'check-section-refs', 'check-option-coverage', 'check-rule-policy-boundary'):
    checks[name] = subprocess.run(['python3', str(root / 'tools/docs' / (name + '.py'))],
        cwd=root, check=True, capture_output=True, text=True).stdout
report = root / 'report-to-andrew/20260908-0955-root-reuse-followup'
log = (report / 'report.log').read_text()
assert 'Output written on report.pdf' in log and 'Overfull' not in log
subprocess.run(['git', 'diff', '--check', '--', '*.md', '*.py', '*.cpp', '*.tex'], cwd=root, check=True)
result = dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), hashes=hashes,
    checks=checks, andrew_compilations=2,
    n1_followup='Existing public explicit-target wrapper returns after reuse,ignoring caller target.5718 passing fixture expects102 for explicit0 atdepth103;this is evidence of current behavior,not target-contract compliance. New isolated successor and focused root/nonroot/clamping/OTFS/proof tests required before promotion. DefaultPolicy target must stay unchanged.',
    test_consideration='Read-only source/fixture review and unchanged complete-stream/timing validators. No new solver behavior. New unit tests are required for the next target-dispatch repair.',
    limitations='Main audit owns pin/result/checker hygiene. Fuzz owner remains opaque;this supplementary helper neither queries nor reads it. No solver change,promotion,push or manual deletion.')
target = root / 'benchmarks/reports/2026-09-08-root-reuse-followup-validation.json'
assert not target.exists()
target.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: late results,public-target follow-up,docs and supplemental report')
