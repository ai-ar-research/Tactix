#!/usr/bin/env python3
"""Check original formulas using actual7267 parent and7302 candidate libraries."""
from pathlib import Path
base = Path(__file__).with_name('check-occurrence-proofs-7255.py')
code = base.read_text()
for before, after in {
    '20260911-7255-occurrence-proofs': '20260911-7304-clause-realloc-proofs',
    "(('previous','7249-single-occurrence'), ('candidate','7253-occurrence-cutover'))":
        "(('previous','7267-dense-realloc'), ('candidate','7302-clause-realloc'))",
}.items():
    assert code.count(before) == 1, before
    code = code.replace(before, after)
marker = '    m = json.loads(manifest.read_text()); bind(manifest)'
assert code.count(marker) == 1
code = code.replace(marker, "    if arm == 'previous': manifest = root / 'benchmarks/runs/20260911-7271-dense-realloc-build/manifest.json'\n" + marker)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
