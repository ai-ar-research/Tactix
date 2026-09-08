#!/usr/bin/env python3
"""Stage only explicit audited normal evidence and the main-owned fuzz resumer."""
from pathlib import Path
source = Path(__file__).with_name('stage-propagation-checkpoint-5667.py').read_text()
source = source.replace('2026-09-08-propagation-cursor-validation.json','2026-09-08-assignment-level-validation.json')
start,end=source.index('paths = {'),source.index('\ndef include')
source=source[:start]+'''paths = {'PROGRESS.md','docs/implementation-notes.md',str(audit.relative_to(root)),
    'benchmarks/reports/2026-09-08-assignment-level-bound.md',
    'benchmarks/reports/2026-09-08-assignment-level-main-ctest.log'}'''+source[end:]
source=source.replace('range(5641, 5668)','range(5668, 5687)')
source=source.replace("if '-5649-' in c.name:","if '-5672-' in c.name:")
source=source.replace("m['review_evidence']","m['predecessor_fixture']")
source=source.replace("('.prefix.tsv', '.boundaries.tsv', '.macro.tsv')","('.prefix.tsv', '.boundaries.tsv', '.macro.tsv', '.state.tsv')")
exec(compile(source,__file__,'exec'))
