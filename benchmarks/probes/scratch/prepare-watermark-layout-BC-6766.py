#!/usr/bin/env python3
"""Predeclare full B28/C5 neutrality gate for the layout-only candidate."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
source = Path(__file__).with_name('prepare-material-schedule-BC-6584.py')
code = source.read_text()
changes = {
    '20260909-6427-activation-stamp-BC-gate': '20260909-6737-reduce-watermark-BC-gate',
    '20260909-6424-activation-stamp-build': '20260909-6730-reduce-watermark-build',
    '20260909-6579-material-schedule-build': '20260909-6763-watermark-layout-build',
    '20260909-6584-material-schedule-BC-gate': '20260909-6766-watermark-layout-BC-gate',
    '20260909-6582-material-proof-control': '20260909-6765-watermark-layout-control',
    "'6424-activation-stamp'": "'6730-reduce-watermark'",
    "'6579-material-schedule'": "'6763-watermark-layout'",
    "'6114-substitute-publication-BC', '6584-material-schedule-BC'":
        "'6737-reduce-watermark-BC', '6766-watermark-layout-BC'",
    'Approved canonical material-flag and Eliminate watermark cutover B28+C5 gate against immutable6424. Stage-one neutral control6574 is a dependency.':
        'Layout-only watermark placement B28+C5 gate against immutable6730. Preserve the historical Reduce repair and all submitted sources.',
    'New flags and the material counter recover missing native invitations and retain pending canonical work. Require all expected verdicts and independent proofs/models, no solve loss or unexplained termination; timing is diagnostic, not acceptance.':
        'Restore watch-and-later member offsets without changing any solver operation. Require exact paired non-timing statistics and certificates, all expected verdicts and independently valid proofs/models. Timing is diagnostic, not acceptance.',
    '66 independently checked/clean records required. Expected solver outcomes permit provisional targeted diagnostics while checks continue; any invalidity pauses dependent work. Changed work/certificates are expected, not a neutrality requirement.':
        '66 independently checked/clean records required. Every paired certificate and non-timing statistic must be identical. Expected outcomes permit provisional continuation while checks run; any invalidity or unexplained work difference pauses dependent interpretation.',
    'benchmarks/runs/20260909-6574-material-state-BC-gate/neutrality-6576.json':
        'benchmarks/runs/20260909-6734-reduce-watermark-proof-control/summary.json'}
for i, (before, after) in enumerate(changes.items()):
    assert before in code, before
    code = code.replace(before, f'__LAYOUT_SUB_{i}__')
for i, after in enumerate(changes.values()):
    code = code.replace(f'__LAYOUT_SUB_{i}__', after)
exec(compile(code, str(source), 'exec'), {'__file__':str(source), '__name__':'__main__'})
