#!/usr/bin/env python3
"""Stage the explicit audited repair and evidence without large streams or fuzz."""
from pathlib import Path
source = Path(__file__).with_name('stage-propagation-checkpoint-5667.py').read_text()
source = source.replace('2026-09-08-propagation-cursor-validation.json', '2026-09-08-root-conflict-reuse-validation.json')
start, end = source.index('paths = {'), source.index('\ndef include')
source = source[:start] + '''paths = {'PROGRESS.md','docs/calculus.md','docs/implementation-notes.md',str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-08-root-conflict-reuse.md',
    'benchmarks/reports/2026-09-08-root-conflict-reuse-main-ctest.log',
    'benchmarks/probes/scratch/reference-root-conflict-reuse-5722.c'}''' + source[end:]
source = source.replace('range(5641, 5668)', 'range(5718, 5740)')
source = source.replace("if '-5649-' in c.name:", "if '-5718-' in c.name:")
source = source.replace("m['review_evidence']", "m['fixed_validation']")
exec(compile(source, __file__, 'exec'))
