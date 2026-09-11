#!/usr/bin/env python3
"""Unchanged B28+C5 gate for the isolated clause-arena allocation change."""
from pathlib import Path
base = Path(__file__).with_name('prepare-factor-arena-BC-7183.py')
code = base.read_text()
changes = {
    '20260910-7183-factor-arena-BC': '20260911-7305-clause-realloc-BC',
    '7183-factor-arena-BC': '7305-clause-realloc-BC',
    '20260910-7166-vivify-revalidation-BC-gate': '20260911-7272-dense-realloc-BC',
    'tactix-factor-arena-BC-v1': 'tactix-clause-realloc-BC-v1',
    "('7181-factor-arena-proofs', '7182-factor-arena-sanitizers')":
        "('7304-clause-realloc-proofs', '7303-clause-realloc-sanitizers')",
    "'benchmarks/runs/20260910-' + stem + '/summary.json'": "'benchmarks/runs/20260911-' + stem + '/summary.json'",
    "stem.startswith('7181')": "stem.startswith('7304')",
    "g['checked'] == 288": "g['checked_UNSAT'] == 156 and g['validated_SAT_models'] == 156",
    '20260910-7180-factor-arena-build': '20260911-7302-clause-realloc-build',
    '7162-vivify-revalidation': '7267-dense-realloc',
    '7180-factor-arena': '7302-clause-realloc',
    "('release', 116), ('bc', 116), ('trace', 115)": "('release', 120), ('bc', 120), ('trace', 119)",
    "bm_path = root / ('benchmarks/runs/20260910-' + stem + '-build/manifest.json')":
        "bm_path = root / ('benchmarks/runs/20260911-' + ('7271-dense-realloc' if arm.startswith('current') else stem) + '-build/manifest.json')",
    "bind(Path(__file__))": """bind(Path(__file__))
bind(Path(__file__).with_name('check-submit-clause-realloc-BC-7305.py'))
bind(root / 'benchmarks/runs/20260911-7299-proc-memory-rank31/resident-analysis-7300.json')
for p in (root / 'benchmarks/runs/20260911-7301-clause-realloc-build/logs').iterdir(): bind(p)
for p in (root / '.claude/worktrees/runtime-clause-realloc-7301/examples').rglob('*'):
    if p.is_file(): bind(p)
""",
    'Exact complete non-timing statistics and certificate bytes/hash across arms.':
        'Exact complete non-timing statistics INCLUDING physical dense counters and certificate bytes/hash across arms. No diagnostic exclusion for allocator-only change.',
    'After all66 exact solver outcomes, three paired31/35/38 runtime waves plus complete Factor profiles. Judge full Factor cycles, whole runtime and RSS; memory growth from arena holes is an explicit risk. Keep telemetry experiment independent; retained7144 is the control.':
        'After all66 exact solver outcomes, three paired9/25/31/35/38 Release waves against actual7267. Primary hypothesis: lower Rank31 peak at later clause-arena growth and fewer copying allocations, without losing9/25 dense savings or worsening runtime. Keep full50 separate; no wait for independent proof completion before provisional work. No generated-code or allocator interaction gain assumed.',
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
