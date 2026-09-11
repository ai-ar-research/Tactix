#!/usr/bin/env python3
"""Predeclare exact-work AND fusion runtime pairs after full BC solver results."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-runtime-7186.py')
code = base.read_text()
changes = {
    '20260910-7186-factor-arena-runtime': '20260911-7205-congruence-fused-runtime',
    '20260910-7183-factor-arena-BC': '20260911-7202-congruence-fused-BC',
    'check-submit-factor-arena-BC-7183.py': 'check-submit-congruence-fused-BC-7202.py',
    '7183-factor-arena-BC': '7202-congruence-fused-BC',
    '7186-factor-arena-runtime': '7205-congruence-fused-runtime',
    "    r = json.loads(p.read_text())\n    assert classify(r, final)":
        "    if not p.exists():\n        print('BC solver outcomes pending; no output created'); sys.exit(0)\n    r = json.loads(p.read_text())\n    assert classify(r, final)",
    'tactix-factor-arena-runtime-v1': 'tactix-congruence-fused-runtime-v1',
    'Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.':
        'Frozen7150 exact fingerprints and7186 matrix/identity controls. Underlying7196 full suites,648 extraction cases,7198 independent proofs and7200 classified sanitizer repair bound through7202. No solver or copied-runner changes.',
    'Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.':
        'Measure whole runtime and RSS for output-location/support loop fusion. Complete Congruence profiles required for unit-cost attribution; no assumed gain from sampled hot PCs. Normal telemetry, arity, logical ticks and scheduling unchanged. Independent of7180arena; no production promotion or fuzz waiver.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
