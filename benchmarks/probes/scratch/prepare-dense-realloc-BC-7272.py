#!/usr/bin/env python3
"""Unchanged B28+C5, complete storage correctness gates, actual7253 control."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-BC-7183.py')
code = base.read_text()
changes = {
    '20260910-7183-factor-arena-BC': '20260911-7272-dense-realloc-BC',
    '7183-factor-arena-BC': '7272-dense-realloc-BC',
    '20260910-7166-vivify-revalidation-BC-gate': '20260911-7256-occurrence-BC',
    'tactix-factor-arena-BC-v1': 'tactix-dense-realloc-BC-v1',
    "('7181-factor-arena-proofs', '7182-factor-arena-sanitizers')":
        "('7269-dense-realloc-proofs', '7268-dense-realloc-sanitizers')",
    "'benchmarks/runs/20260910-' + stem + '/summary.json'": "'benchmarks/runs/20260911-' + stem + '/summary.json'",
    "stem.startswith('7181')": "stem.startswith('7269')",
    "g['checked'] == 288": "g['checked_UNSAT'] == 156 and g['validated_SAT_models'] == 156",
    '20260910-7180-factor-arena-build': '20260911-7271-dense-realloc-build',
    '7162-vivify-revalidation': '7253-occurrence-cutover',
    '7180-factor-arena': '7267-dense-realloc',
    "('release', 116), ('bc', 116), ('trace', 115)": "('release', 119), ('bc', 119), ('trace', 118)",
    "bm_path = root / ('benchmarks/runs/20260910-' + stem + '-build/manifest.json')":
        "bm_path = root / ('benchmarks/runs/20260911-' + ('7271-dense-realloc' if arm.startswith('candidate') else stem) + '-build/manifest.json')",
    "bind(Path(__file__))": "bind(Path(__file__))\nbind(authority / 'support-repair.json')\nbind(Path(__file__).with_name('check-submit-dense-realloc-BC-7272.py'))\nbind(Path(__file__).with_name('occurrence-fingerprint-7256.py'))",
    'Exact complete non-timing statistics and certificate bytes/hash across arms.':
        'Exact complete non-timing statistics INCLUDING physical dense slice moves/defrags, and certificate bytes/hash across arms. Allocation changes no logical order, capacity, offset or physical list operation. No diagnostic exclusion.',
    'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Factor profiles. Judge full Factor cycles, whole runtime and RSS; memory growth from arena holes is an explicit risk. Keep telemetry experiment independent; retained7144 is the control.':
        'After all66 exact solver outcomes, three independent paired9/25/31/35/38 Release waves against actual7253. Prioritize peak RSS on9/25 and preserve whole runtime on31/35/38; all physical/work/certificate fingerprints exact. Current7265 capacity-growth intervals support the transient-copy hypothesis but do not promise realloc grows in place. Full50/checks/fuzz/consolidation remain separate. No wait for background proofs before provisional development.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
