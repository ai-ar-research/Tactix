#!/usr/bin/env python3
"""Freeze the normal-runtime checkpoint without inspecting fuzz-owned resources."""
from pathlib import Path

directory = Path(__file__).resolve().parent
source = (directory / 'freeze-root-followup-5435.py').read_text()
changes = {
    '5433:1669971,5434:1669972,5435:1669973}':
        '5433:1669971,5434:1669972,5435:1669973,5436:1669994,5437:1670050,5438:None,5440:1670118,5441:1670119}',
    "['squeue','-u','hwu','-h','-o','%i %T %j']":
        "['squeue','-j',','.join(str(j) for j in jobs.values() if j),'-h','-o','%i %T %j']",
    'if number in (5424,5433):': 'if number in (5424,5433,5436,5437):',
    "('check-doc-size','check-progress','check-section-refs','check-option-coverage','check-rule-policy-boundary')":
        "('check-doc-size','check-section-refs','check-option-coverage','check-rule-policy-boundary')",
    "log=root/'build/release/Testing/Temporary/LastTest.log'":
        """import importlib.util
spec=importlib.util.spec_from_file_location('progress_check', root/'tools/docs/check-progress.py')
progress_check=importlib.util.module_from_spec(spec);spec.loader.exec_module(progress_check)
errors=[];warnings=[];notes=[]
progress_check.check_citations(errors,warnings,notes)
assert not errors,errors
checks['check-progress-scoped']=dict(citation_errors=errors,citation_warnings=warnings,
    limitation='Full queue/build scans deferred: they inspect Claude-owned fuzz resources. Explicit non-fuzz runtime jobs and source/pin hashes audited separately.')
log=root/'build/release/Testing/Temporary/LastTest.log'""",
    '2026-09-07-root-entry-main-ctest.log': '2026-09-07-vivify-max-main-ctest.log',
    '2026-09-07-root-entry-validation.json': '2026-09-07-vivify-max-validation.json',
    '20260907-2055-root-entry': '20260907-2144-vivify-max',
    "for n in (1,2):assert 'Output written on report.pdf' in (report/f'compile-{n}.log').read_text()":
        "assert 'Output written on report.pdf' in (report/'report.log').read_text() # Both successful invocations retained in primary tool transcript.",
    'seconds=6.38': 'seconds=5.22',
    'Incomplete views do not establish an aggregate or acceptance; pending expected results guide dependent work.':
        'Only forty complete pairs establish the provisional full aggregate. Pending expected results guide investigation, not final acceptance.',
    'Production unchanged; independent metadata 5426 and semantic root-entry 5430 remain UNFUZZED and unpromoted.':
        'Production unchanged. Vivify 5438 preserves actual old/new order and pose in 144 cases; matching builds pass 79/79. Runtime/BC queued; Claude-owned background fuzz pending; no promotion.',
    'User approval requested for separate future solver/checker jobs; no contract or runner change made.':
        'User authorized periodic background fuzzing during independent development; plan updated. Separate future solver/checker jobs remain unapproved; no runner contract change.',
    'Fixed tests, normal runtime and bounded diagnostics only; no new fuzz campaign/artifact inspected. Current jobs and user allocation untouched; prior push rejection remains in force.':
        'Fixed tests, explicit non-fuzz runtime jobs and bounded diagnostics only. Background Claude owner session 30094 is pending; no fuzz job/artifact inspected. No manual deletion, production promotion or push. User allocation untouched.'
}
for old, new in changes.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
exec(compile(source, __file__, 'exec'))
