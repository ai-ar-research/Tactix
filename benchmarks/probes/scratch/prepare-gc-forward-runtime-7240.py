#!/usr/bin/env python3
"""Predeclare matched GC-forward timings only after all66 exact BC outcomes."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-runtime-7186.py')
code = base.read_text()
for before,after in {
    '20260910-7186-factor-arena-runtime':'20260911-7240-gc-forward-runtime',
    '20260910-7183-factor-arena-BC':'20260911-7238-gc-forward-BC',
    'check-submit-factor-arena-BC-7183.py':'check-submit-gc-forward-BC-7238.py',
    '7183-factor-arena-BC':'7238-gc-forward-BC',
    '7186-factor-arena-runtime':'7240-gc-forward-runtime',
    "    r = json.loads(p.read_text())\n    assert classify(r, final)":
        "    if not p.exists():\n        print('BC solver outcomes pending; no output created'); sys.exit(0)\n    r = json.loads(p.read_text())\n    assert classify(r, final)",
    'tactix-factor-arena-runtime-v1':'tactix-gc-forward-runtime-v1',
    'Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.':
        'Frozen7150 fingerprints and7186 matrix/identity controls. Full7233 suites117/117/116,26240 watch transactions,896 actual parent collector states,7234 six sanitizer fixtures twice,7235 fixed216proofs/216models and7237 additional16 root-lock proofs bound through7238. No changes to source or copied runtime runner.',
    'Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.':
        'Measure complete GC/Reduce cost and whole runtime/RSS for fused sparse watch filtering/forwarding. Normal telemetry, semantics, scheduling, advice and budgets retained. Direct7144 base independent of root-binary, AND and arena prototypes. No production integration, default promotion or fuzz waiver.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
