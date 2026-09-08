#!/usr/bin/env python3
"""Freeze explicit normal-campaign evidence; keep fuzz ownership opaque."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start,end = source.index('jobs={'),source.index('\ndef sha')
source = source[:start]+'''jobs={5651:1672791,5652:None,5664:1672864,5669:1672950,
    5672:None,5677:1672959,5678:1672960,5679:None,5687:1672962,5689:1672970}'''+source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace('if number in (5437,5449,5453):', 'if number in (5669,5677,5689):')
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),*(json.loads(p.read_text())['job'] for n in (5652,5679) for c in (root/'benchmarks/runs').glob(f'20260908-{n}-*') for p in c.glob('wave*-submission.json'))])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-08-choice-and-ownership-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget','20260908-0820-choice-and-ownership')
source = source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-08-choice-and-ownership-validation.json')
source = source.replace('seconds=5.32','seconds=6.07')
start,end = source.index('    source_status='),source.index('\n    manual_removed_bytes=')
source = source[:start]+"""    source_status='No solver changes.5669 full parent replay identifies stable score heap,not unavailable120438/focused queue;5689job1672970 follows.5687job1672962 memory ownership pending.5652 exact-work three pairs median0.9973786590x5626;4/6 checked at frozen snapshot.5677 exact full1M replayPASS;5678BC pending,5679timing prepared. Goal unmet,no promotion.',
    fuzz_owner='5657 owner95829 polled and live;only CLI response observed,no independent fuzz job/artifact inspection. Covers5649,not5672. No duplicate campaign.',
""".rstrip()+source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No correctness incident. No manual deletion; unique source/pin/trace authority retained.',")
source = source.replace('result=dict(observed_at=', 'result=dict(fixed_authorities={},observed_at=')
anchor = 'checks={name:run('
assert source.count(anchor) == 1
source = source.replace(anchor, """fixture_checks={}
for script in ('compare-rank33-choice-5670.py','validate-rank39-memory-5688.py','compare-rank33-heap-5690.py'):
    p=root/'benchmarks/probes/scratch'/script
    fixture_checks[script]=dict(sha256=sha(p),output=run(['python3',str(p),'--self-test']))
"""+anchor)
source = source.replace('campaigns=campaigns,checks=checks,', 'campaigns=campaigns,checks=checks,fixture_checks=fixture_checks,')
exec(compile(source,__file__,'exec'))
