#!/usr/bin/env python3
"""Audit explicit non-fuzz source, test, runtime and report authorities."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start,end=source.index('jobs={'),source.index('\ndef sha')
source=source[:start]+'''jobs={5604:1672600,5628:1672704,5629:None,5651:1672791,
    5652:None,5664:1672864,5669:1672950,5672:None,5677:1672959,5678:1672960,5679:None}'''+source[end:]
source=source.replace("f'20260907-{number}-*'","f'20260908-{number}-*'")
source=source.replace('if number in (5437,5449,5453):','if number in (5669,5677):')
source=source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),*(json.loads(p.read_text())['job'] for n in (5629,5652,5679) for c in (root/'benchmarks/runs').glob(f'20260908-{n}-*') for p in c.glob('wave*-submission.json'))])")
source=source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-08-assignment-level-main-ctest.log')
source=source.replace('20260907-2255-eliminate-budget','20260908-0800-assignment-level')
source=source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-08-assignment-level-validation.json')
source=source.replace('seconds=5.32','seconds=5.87')
start,end=source.index('    source_status='),source.index('\n    manual_removed_bytes=')
source=source[:start]+"""    source_status='5672 exact antecedent maximum stops at current-level bound/root zero;90/90 twice,89/89 trace;3288 actual old/new states equal.5677replay/5678BC submitted,5679timing prepared.5669 choice probe pending. Full5664 all80solves,1.1283688258212774xKissat,59checked at frozen snapshot;losses33/31/37,memory remains open;goal not met,no promotion.',
    fuzz_owner='5657 owner33894 returned submission-only,notPASS;resumed same campaign/conversation under95829,only terminal response observed. Covers5649,not5672. Prior5610PASS5592 retained;no independent fuzz jobs/artifact/report inspection.',
""".rstrip()+source[end:]
source=source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No correctness failure or repair escalation. Annotation5671 deleted only its own generated temporary expansions;original compressed profiles/pins retained. No material user data deleted.',")
anchor='result=dict(observed_at='
source=source.replace(anchor,"""fixed_authorities={}
for c in [root/'benchmarks/runs/20260908-5671-propagation-annotation',root/'benchmarks/runs/20260908-5672-assignment-level/predecessor-fixture']:
    for p in c.rglob('*'):
        if p.is_file() and p.suffix in ('.json','.log','.txt','.stderr'):
            fixed_authorities[str(p.relative_to(root))]=sha(p)
"""+anchor)
source=source.replace('result=dict(observed_at=','result=dict(fixed_authorities=fixed_authorities,observed_at=')
exec(compile(source,__file__,'exec'))
