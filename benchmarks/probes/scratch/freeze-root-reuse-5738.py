#!/usr/bin/env python3
"""Audit the root-reuse increment and explicit normal jobs; fuzz remains opaque."""
from pathlib import Path
source = Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start, end = source.index('jobs={'), source.index('\ndef sha')
source = source[:start] + '''jobs={5664:1672864,5672:None,5678:1672960,5679:None,
    5687:1672962,5704:1673060,5710:1673072,5712:1673074,5718:None,
    5726:1673168,5727:1673169,5728:None,5735:1673171}''' + source[end:]
source = source.replace("f'20260907-{number}-*'", "f'20260908-{number}-*'")
source = source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),*(json.loads(p.read_text())['job'] for n in (5679,5728) for c in (root/'benchmarks/runs').glob(f'20260908-{n}-*') for p in c.glob('wave*-submission.json'))])")
source = source.replace('if number in (5437,5449,5453):', 'if number in (5704,5710,5726,5735):')
anchor = "                assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']"
assert source.count(anchor) == 1
source = source.replace(anchor, """                assert r['scratch_deleted'] and r['partial_certificate']['bytes']
                if number in (5704,5710):
                    assert r['exit_code']==124 and r['command'][:2]==['timeout','1200']
                    assert not (c/'traces'/(r['arm']+'.prefix.tsv')).exists()
                    assert not (c/'summary.json').exists()
                else:assert r['exit_code']==0""")
source = source.replace('campaigns=[]', 'campaigns=[]\nfixed_authorities={}')
anchor = "    results=sorted((c/'results').glob('*.json'))"
assert source.count(anchor) == 1
source = source.replace(anchor, """    for section in ('fixed_validation','initial_fixture','predecessor_fixture'):
        for rel,digest in m.get(section,{}).items():
            assert sha(root/rel)==digest,(number,rel)
            if section!='predecessor_fixture':fixed_authorities[rel]=digest
    if number==5718:
        fixed=json.loads((c/'fixed-validation/summary.json').read_text())
        assert fixed['predecessor_cases']==288 and fixed['native_cases']==24
        assert fixed['all_native_states_equal'] and fixed['all24_selected_previous_states_differ']
        assert len(fixed['micro_proofs'])==288
        for arm,count in [('release',91),('bc',91),('trace',90)]:
            assert f'100% tests passed out of {count}' in (c/'logs'/f'ctest-r1-{arm}.log').read_text()
""" + anchor)
source = source.replace('2026-09-07-eliminate-budget-main-ctest.log', '2026-09-08-root-conflict-reuse-main-ctest.log')
source = source.replace('20260907-2255-eliminate-budget', '20260908-0945-root-reuse')
source = source.replace('2026-09-07-eliminate-budget-validation.json', '2026-09-08-root-conflict-reuse-validation.json')
source = source.replace('seconds=5.32', 'seconds=5.19')
source = source.replace('assert not target.exists();shutil.copy2(main,target)',
    "if target.exists():assert sha(target)==sha(main)\nelse:shutil.copy2(main,target)")
source = source.replace('campaigns=campaigns,checks=checks,', 'campaigns=campaigns,checks=checks,fixed_authorities=fixed_authorities,')
start, end = source.index('    source_status='), source.index('\n    manual_removed_bytes=')
source = source[:start] + """    source_status='Production unchanged.5718 root reuse91/91 Release twice,90/90 trace;288 predecessor cases,24 native matches,288 checked micro-proofs.5726 native full replayPASS;known182093 split moves227618 after23612137 common suffix rows.5712 all40 pairs1.110877xKissat;all40 parent work/proofs equal.5679 median1.030478x with6.01% candidate spread,no established unit-cost gain.5687 checked/clean. Goal unmet,no promotion.',
    fuzz_owner='5657/95829 terminalPASS5649:70000 cases,63573 checked verdicts,zero solver failures,312 reachability cases.5715/4746 live on5672,not5718.Only CLI responses observed,no independent fuzz job/artifact inspection.',
""".rstrip() + source[end:]
source = source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual deletion. Fixed validator removed288 generated CNF/proof pairs after independent verification;hashes/logs retained.5704/5710 own timeout scratch removed. Unique sources,pins,traces retained.',")
exec(compile(source, __file__, 'exec'))
