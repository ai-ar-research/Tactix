#!/usr/bin/env python3
"""Preserve the existing B28+C5 gate and runner for the approved view prototype."""
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
proof = json.loads((root / 'benchmarks/runs/20260910-7095-bve-view-proofs/summary.json').read_text())
assert proof['checked'] == 96 and proof['exact_parent_certificates']
assert (root / 'benchmarks/runs/20260910-7090-bve-view-sanitizers/summary.json').exists()
source = Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code = source.read_text()
changes = {
 '20260909-6427-activation-stamp-BC-gate': '20260910-7034-collect-retry-BC-gate',
 '20260909-6424-activation-stamp-build': '20260910-7030-collect-retry-build',
 '20260909-6579-material-schedule-build': '20260910-7087-bve-views-build',
 '20260909-6584-material-schedule-BC-gate': '20260910-7091-bve-views-BC-gate',
 '20260909-6582-material-proof-control': '20260910-7095-bve-view-proofs',
 "'6424-activation-stamp'": "'7030-collect-retry'",
 "'6579-material-schedule'": "'7087-bve-views'",
 "'6114-substitute-publication-BC', '6584-material-schedule-BC'": "'7034-collect-retry-BC', '7091-bve-views-BC'",
 "root / 'tools/bench/runtime_task.py'": "old / 'pins/runtime_task.py'",
 'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
   'Approved BVE view stage1 against immutable7030; unchanged full B28 and predeclared C5, frozen runner and resource contract.',
 'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
   'Avoid materializing root-free large BVE antecedents with stable arena offsets; retain root-filtered snapshots. Require exact cross-arm work/certificates, all expected outcomes and checked clean records. No scheduling/kernel-layout/public API change.',
 'Changed work/certificates are expected, not a neutrality requirement.': 'Exact cross-arm work/certificates required; differences fail the cost-only premise.',
 'benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json': 'benchmarks/runs/20260910-7090-bve-view-sanitizers/summary.json',
}
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__VIEW_{i}__')
for i, after in enumerate(changes.values()): code = code.replace(f'__VIEW_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
