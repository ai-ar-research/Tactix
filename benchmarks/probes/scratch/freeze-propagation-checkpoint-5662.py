#!/usr/bin/env python3
"""Audit only explicit normal campaigns, preserving the independent fuzz owner."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start, end = source.index('jobs={'), source.index('\ndef sha')
source = source[:start] + '''jobs={5599:None,5604:1672600,5628:1672704,5629:None,
    5639:1672751,5644:1672779,5649:None,5650:1672790,5651:1672791,5652:None,5664:1672864}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace('if number in (5437,5449,5453):', 'if number in (5639,5644,5650):')
source = source.replace("assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']",
    "assert r['exit_code']==(124 if number==5644 else 0) and r['scratch_deleted'] and r['partial_certificate']['bytes']")
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j), '1672596','1672598','1672681',*(json.loads(p.read_text())['job'] for n in (5629,5652) for c in (root/'benchmarks/runs').glob(f'20260908-{n}-*') for p in c.glob('wave*-submission.json'))])")
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-propagation-cursor-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0719-propagation-cursor')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-propagation-cursor-validation.json')
source = source.replace('seconds=5.32', 'seconds=5.96')
start, end = source.index('    source_status='), source.index('\n    manual_removed_bytes=')
source = source[:start] + """    source_status='5649 local cursor/per-call specialization passes89/89 twice,88/88 trace;224 actual old/new states equal.5650 complete1M traces/proof identical.5651BC,5652timing and5664full run.5663 validates timed-out5644 selected window:2574matchingseeds,170sameunits,both targetvars fixed,no missing Sweep search work established. Full current5592 remains1.13758865xKissat;goal not met,no promotion.',
    fuzz_owner='5610 terminalPASS on5592:50000 cases,45752 checked verdicts,0failures,300corpus,144directreachability. Owner1814 closed. New5657 owns5649 under33894;only terminal CLI handles/responses observed,no independent fuzz resource/report audit.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='Claude5647 terminalPASS: fixture called null raw registry instances pointer;test-only repair,solver unchanged. No reviewer or parent deletion. Original5641 segfault source/build/logs retained. Earlier compile API typo and strict observer framing failures preserved;neither is a failed solver oracle.',")
anchor = 'result=dict(observed_at='
source = source.replace(anchor, """fixed_authorities={}
for c in [root/'benchmarks/runs/20260908-5641-propagation-cursor', root/'benchmarks/runs/20260908-5647-propagation-cursor-repair']:
    for p in c.rglob('*'):
        if p.is_file() and p.name != 'test_propagation_cursor_predecessor':
            fixed_authorities[str(p.relative_to(root))]=sha(p)
""" + anchor)
source = source.replace('result=dict(observed_at=', 'result=dict(fixed_authorities=fixed_authorities,observed_at=')
exec(compile(source, __file__, 'exec'))
