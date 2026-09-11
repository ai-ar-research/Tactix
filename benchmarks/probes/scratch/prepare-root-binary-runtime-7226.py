#!/usr/bin/env python3
"""Predeclare root-binary exact-work runtime repetitions after66 BC outcomes."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-runtime-7186.py')
code = base.read_text()
for before,after in {
    '20260910-7186-factor-arena-runtime':'20260911-7226-root-binary-runtime',
    '20260910-7183-factor-arena-BC':'20260911-7221-root-binary-BC',
    'check-submit-factor-arena-BC-7183.py':'check-submit-root-binary-BC-7221.py',
    '7183-factor-arena-BC':'7221-root-binary-BC',
    '7186-factor-arena-runtime':'7226-root-binary-runtime',
    "    r = json.loads(p.read_text())\n    assert classify(r, final)":
        "    if not p.exists():\n        print('BC solver outcomes pending; no output created'); sys.exit(0)\n    r = json.loads(p.read_text())\n    assert classify(r, final)",
    'tactix-factor-arena-runtime-v1':'tactix-root-binary-runtime-v1',
    'Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.':
        'Frozen7150 fingerprints and7186 matrix/identity controls. Full7218 suites,5400-state exact collector oracle,7220 sanitizer fixtures twice and7224 valid fixed proof/model controls bound through7221.7219/7222 fixture failures preserved and classified; no solver or copied-runner changes.',
    'Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.':
        'Measure complete Reduce and nested sparse-collection cost, whole runtime and RSS for root-true binary discovery. Normal telemetry, semantics, scheduling, advice and budgets retained. Independent of AND and Factor-arena prototypes; no production integration, default promotion or fuzz waiver.',
}.items():
    assert before in code,before
    code = code.replace(before,after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
