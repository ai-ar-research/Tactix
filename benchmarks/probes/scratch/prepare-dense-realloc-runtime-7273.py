#!/usr/bin/env python3
"""Three independent five-witness waves, after all BC solver outcomes."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-runtime-7186.py')
code = base.read_text()
for before, after in {
    '20260910-7186-factor-arena-runtime':'20260911-7273-dense-realloc-runtime',
    '20260910-7183-factor-arena-BC':'20260911-7272-dense-realloc-BC',
    'check-submit-factor-arena-BC-7183.py':'check-submit-dense-realloc-BC-7272.py',
    'submit-vivify-compact-wave-7150.py':'occurrence-fingerprint-7256.py',
    'h.fingerprint(':'h.raw_fingerprint(',
    '20260910-7167-vivify-revalidation-ranks1-50':'20260911-7262-occurrence-ranks1-50',
    'for rank in (31, 35, 38):':'for rank in (9, 25, 31, 35, 38):',
    "r['verdict'] == r['expected'] == 'UNSAT'":"r['verdict'] == r['expected'] and r['expected'] in ('SAT', 'UNSAT')",
    "expected='UNSAT'))":"expected=r['expected']))",
    "    r = json.loads(p.read_text())\n    assert classify(r, final)":
        "    if not p.exists():\n        print('BC outcomes incomplete; no campaign created'); sys.exit(0)\n    r = json.loads(p.read_text())\n    assert classify(r, final)",
    "bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))":
        "bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))\nbind(Path(__file__).with_name('check-submit-dense-realloc-runtime-7273.py'))\nbind(root/'benchmarks/runs/20260911-7265-occurrence-memory/summary-7266.json')",
    "replace('7183-factor-arena-BC', '7186-factor-arena-runtime')":"replace('7256-occurrence-BC', '7273-dense-realloc-runtime')",
    "'--array=0-17'":"'--array=0-29'",
    "array='0-17'":"array='0-29'",
    'tactix-factor-arena-runtime-v1':'tactix-dense-realloc-runtime-v1',
    'Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.':
        '7271 full119/119/118 suites, actual-parent state/capacity replay,6 intentional fatal controls;7268 twenty sanitizer legs;7269 156proofs/156models/2negative controls. Strict7256 raw fingerprint retains physical counts; new7273 matrix/wave/identity rejection controls. Runner unchanged.',
    'Every outcome expected, every non-timing statistic and certificate bytes/hash exact retained7144 and own repeats.':
        'Every outcome expected, every non-timing statistic INCLUDING physical dense moves/defrags and certificate bytes/hash exact own7253 Release and own repeats.',
    'Three independent paired31/35/38 six-task waves, reversed middle order.':
        'Three independent paired9/25/31/35/38 ten-task waves, reversed middle order.',
    'Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.':
        'Primary metric: peak-RSS reduction on current largest unshelved excess-memory witnesses9/25; current7265 first peaks follow512MiB-to1GiB payload growth. Also measure whole runtime and RSS on31/35/38. Preserve capacities/order; no net gain assumed and no arbitrary minimum gain. Trace timings are not scored. Normal telemetry unchanged. Complete-profile diagnosis if results conflict; full50, independent checks, fuzz and consolidation still required. No promotion.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
