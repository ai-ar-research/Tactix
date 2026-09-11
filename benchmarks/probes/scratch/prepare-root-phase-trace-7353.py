#!/usr/bin/env python3
"""Refine the first B14 Eliminate difference with existing bounded observers."""
from pathlib import Path
base=Path(__file__).with_name('prepare-root-decision-trace-7349.py')
code=base.read_text()
for before,after in {
    '7349-root-decision-trace':'7353-root-phase-trace',
    'check-submit-root-decision-trace-7349.py':'check-submit-root-phase-trace-7353.py',
    "('decisions', 'macro')":"('decisions', 'macro', 'active', 'inprocess')",
    "os.environ['TACTIX_MACRO_BOUNDARY_TRACE'] = str(trace_files['macro'])":
        "os.environ['TACTIX_MACRO_BOUNDARY_TRACE'] = str(trace_files['macro'])\n        os.environ['TACTIX_MACRO_ACTIVE_SET'] = str(trace_files['active'])\n        os.environ['SAT_MACRO_ACTIVE_SET_EVENT'] = 'eliminate_end'\n        os.environ['SAT_MACRO_ACTIVE_SET_CONFLICT'] = '26295'\n        os.environ['TACTIX_INPROCESS_BOUNDARY_TRACE'] = str(trace_files['inprocess'])\n        os.environ['SAT_INPROCESS_BOUNDARY_CONFLICT_FROM'] = '26295'",
    "del os.environ['TACTIX_MACRO_BOUNDARY_TRACE']":
        "del os.environ['TACTIX_MACRO_BOUNDARY_TRACE']\n        for name in ('TACTIX_MACRO_ACTIVE_SET','SAT_MACRO_ACTIVE_SET_EVENT','SAT_MACRO_ACTIVE_SET_CONFLICT','TACTIX_INPROCESS_BOUNDARY_TRACE','SAT_INPROCESS_BOUNDARY_CONFLICT_FROM'): del os.environ[name]",
    "for path in [Path(__file__),Path(__file__).with_name('check-submit-root-phase-trace-7353.py')]: bind(path)":
        "for path in [Path(__file__),Path(__file__).with_name('prepare-root-decision-trace-7349.py'),Path(__file__).with_name('check-submit-root-decision-trace-7349.py'),Path(__file__).with_name('check-submit-root-phase-trace-7353.py'),root/'benchmarks/reports/2026-09-11-root-B14-analysis-7352.json']: bind(path)",
    '7348 isolates B14 changed work to root folding, not collector connection repair. Locate the earliest changed portable decision and preceding macro boundary on complete independent solves using already-built trace pins.':
        '7352 locates 1343955 identical choices, first changed choice C26860/D1307488 after changed restart level. Earlier Eliminate-end C26295 has candidate one fewer active unassigned variable at identical propagations. Dump permanent active sets at that exact end plus inprocessing slot boundaries from C26295; preserve complete7349 choice/macro streams and own Release work/proof.',
    'No solver edit; existing actual trace functions, old runtime measurement/checking contract. New runner only sets two local trace paths and archives them after solver exit. Identity/replay checks and full compressed trace hashes bind the analysis.':
        'No solver edit; add existing one-shot permanent active-set and per-slot boundary consumers. No global event trace or additional stop. Require exact entire7349 decision and macro replay plus original proof/work before interpreting new fields. Parser rejects mixed identities, malformed rows and missing exact-conflict snapshot.',
}.items():
    assert before in code,before; code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))
