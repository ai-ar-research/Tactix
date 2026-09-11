#!/usr/bin/env python3
"""Three independent five-witness waves after complete BC solver outcomes."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-runtime-7186.py')
code = base.read_text()
changes = {
    '20260910-7186-factor-arena-runtime': '20260911-7306-clause-realloc-runtime',
    '20260910-7183-factor-arena-BC': '20260911-7305-clause-realloc-BC',
    'check-submit-factor-arena-BC-7183.py': 'check-submit-clause-realloc-BC-7305.py',
    'submit-vivify-compact-wave-7150.py': 'occurrence-fingerprint-7256.py',
    'h.fingerprint(': 'h.raw_fingerprint(',
    '20260910-7167-vivify-revalidation-ranks1-50': '20260911-7283-realloc-ranks1-50',
    'for rank in (31, 35, 38):': 'for rank in (9, 25, 31, 35, 38):',
    "r['verdict'] == r['expected'] == 'UNSAT'": "r['verdict'] == r['expected'] and r['expected'] in ('SAT', 'UNSAT')",
    "expected='UNSAT'))": "expected=r['expected']))",
    "    r = json.loads(p.read_text())\n    assert classify(r, final)":
        "    if not p.exists():\n        print('BC outcomes incomplete; no campaign created'); sys.exit(0)\n    r = json.loads(p.read_text())\n    assert classify(r, final)",
    "bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))":
        "bind(gate / 'manifest.json'); bind(gate / 'launch.json'); bind(reader); bind(Path(__file__))\nbind(Path(__file__).with_name('check-submit-clause-realloc-runtime-7306.py'))",
    "replace('7183-factor-arena-BC', '7186-factor-arena-runtime')":
        "replace('7272-dense-realloc-BC', '7306-clause-realloc-runtime')",
    "'--array=0-17'": "'--array=0-29'", "array='0-17'": "array='0-29'",
    'tactix-factor-arena-runtime-v1': 'tactix-clause-realloc-runtime-v1',
    'Frozen7150 exact fingerprint fixtures plus new7186 matrix/identity controls; underlying arena unit/proof/sanitizer gates bound through7183. No changes to solver or copied runtime runner.':
        '7302 full120/120/119 suites; exact actual-parent32768-clause58-growth32-collection raw arena replay,52-case364-boundary rule replay,840 replacements.7303 twenty sanitizer legs;7304 156proofs/156models/two rejecting controls. Strict raw fingerprint retains all physical counts; inherited30-row/wave/identity rejection controls. Unchanged scored runtime runner; no strace or proc observer.',
    'Every outcome expected, every non-timing statistic and certificate bytes/hash exact retained7144 and own repeats.':
        'Every outcome expected, every non-timing statistic INCLUDING physical counters and certificate bytes/hash exact actual7267 full7283 and own repeats.',
    'Three independent paired31/35/38 six-task waves, reversed middle order.':
        'Three independent paired9/25/31/35/38 ten-task waves, reversed middle order.',
    'Measure whole runtime and memory including arena holes. Complete Factor profiles are also required for structural unit-cost attribution. Normal telemetry retained. No production integration, default promotion or fuzz waiver.':
        'Primary gate: reduce Rank31 transient clause-arena-copy peak without losing Rank9/25 dense-memory gains; retain every runtime/RSS regression on9/25/31/35/38. Three paired medians/full ranges, not best-of. No benefit assumed from realloc; unchanged collection and allocator history may retain memory. Complete profiles/external diagnostics if outcomes conflict; full50/independent checks/fuzz/consolidation remain required before acceptance.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
