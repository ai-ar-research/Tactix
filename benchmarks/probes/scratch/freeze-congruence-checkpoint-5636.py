#!/usr/bin/env python3
"""Audit validated cleanup source and explicit normal gates; leave fuzz ownership opaque."""
from pathlib import Path
source=Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start,end=source.index('jobs={'),source.index('\ndef sha')
source=source[:start]+'''jobs={5598:1672529,5599:None,5604:1672600,5611:1672685,
    5613:1672687,5626:None,5627:1672703,5628:1672704,5629:None,5634:1672707,5639:1672751}'''+source[end:]
source=source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source=source.replace('if number in (5437,5449,5453):','if number in (5611,5613,5627,5634,5639):')
source=source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),'1672596','1672598','1672681',*(json.loads(p.read_text())['job'] for p in (root/'benchmarks/runs/20260908-5629-congruence-index-rank33-runtime').glob('wave*-submission.json'))])")
source=source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-08-congruence-index-main-ctest.log')
source=source.replace('20260907-2255-eliminate-budget','20260908-0639-congruence-index')
source=source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-08-congruence-index-validation.json')
source=source.replace('seconds=5.32','seconds=6.13')
start,end=source.index('    source_status='),source.index('\n    manual_removed_bytes=')
source=source[:start]+"""    source_status='Validated5626 source implements visited root-satisfied index cleanup;88/88 twice,87/87 trace,1200 actual old/new cases,75 native matches,24 changes.5627 closes five clauses with31,494,584 common decisions;5634 still splits182093.5638 selects Sweep120330;5639 runs with10 parser fixtures. BC5628 andtiming5629 wave0/job1672776 run. Parent5598 now66/66 checked;full5592 remains1.13758865xKissat;goal not met. No promotion.',
    fuzz_owner='Claude5610 original78639 returned submission-only;resumed same conversation under1814,still owns5592 not5626. Claude5539 priorPASS on5522 is authoritative. Only owner CLI terminal responses/handles observed,no fuzz jobs/artifacts or independent report recomputation.',
""".rstrip()+source[end:]
source=source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='Claude5623 terminalPASS: bad DRAT order expectation,not solver/proof defect;solver source unchanged,corrected fixture retains exact byte/permutation/id checks. Reviewer removed its debug binary and4 inspected stale test temp files;parent manual deletion0. Original5619 failed prototype preserved. Original5618 compile typo and5612 missing __file__ framing errors retained,not solver failures.',")
anchor="result=dict(observed_at="
assert source.count(anchor)==1
source=source.replace(anchor,"""extra_paths=[root/'benchmarks/runs/20260908-5618-congruence-index-fixture/summary.json',
    root/'benchmarks/runs/20260908-5619-congruence-index/manifest.json',
    root/'benchmarks/runs/20260908-5623-congruence-index-repair/REPORT.md']
for folder in ('checker','logs'):
    extra_paths += [p for p in (root/'benchmarks/runs/20260908-5623-congruence-index-repair'/folder).iterdir() if p.is_file()]
fixed_authorities={str(p.relative_to(root)):sha(p) for p in extra_paths}
"""+anchor)
source=source.replace('result=dict(observed_at=', 'result=dict(fixed_authorities=fixed_authorities,observed_at=')
exec(compile(source,__file__,'exec'))
