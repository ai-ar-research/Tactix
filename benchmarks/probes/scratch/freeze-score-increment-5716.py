#!/usr/bin/env python3
"""Audit explicit normal campaigns and immutable observer sources; fuzz opaque."""
from pathlib import Path
source=Path(__file__).with_name('freeze-eliminate-checkpoint-5455.py').read_text()
start,end=source.index('jobs={'),source.index('\ndef sha')
source=source[:start]+'''jobs={5651:1672791,5652:None,5664:1672864,5672:None,
    5678:1672960,5679:None,5687:1672962,5694:1672973,5696:None,
    5698:1672988,5702:1672996,5704:1673060,5710:1673072,5712:1673074}'''+source[end:]
source=source.replace("f'20260907-{number}-*'","f'20260908-{number}-*'")
source=source.replace("','.join(str(j) for j in jobs.values() if j)",
    "','.join([*(str(j) for j in jobs.values() if j),*(json.loads(p.read_text())['job'] for n in (5652,5679) for c in (root/'benchmarks/runs').glob(f'20260908-{n}-*') for p in c.glob('wave*-submission.json'))])")
source=source.replace('if number in (5437,5449,5453):','if number in (5694,5698,5702,5704,5710):')
anchor="                assert r['exit_code']==0 and r['scratch_deleted'] and r['partial_certificate']['bytes']"
assert source.count(anchor)==1
source=source.replace(anchor,"""                assert r['scratch_deleted'] and r['partial_certificate']['bytes']
                if number==5704:
                    assert r['exit_code']==124 and r['command'][:2]==['timeout','1200']
                    assert not (c/'traces'/(r['arm']+'.prefix.tsv')).exists()
                    assert not (c/'summary.json').exists()
                else:assert r['exit_code']==0""")
anchor="    results=sorted((c/'results').glob('*.json'))"
source=source.replace(anchor,"""    for arm,entry in m.get('sources',{}).items():
        for rel,digest in entry['files'].items():assert sha(root/entry['source']/rel)==digest
        assert sha(c/(arm+'.patch'))==entry['patch_sha256']
    if 'cumulative_patch_sha256' in m:assert sha(c/'cumulative.patch')==m['cumulative_patch_sha256']
"""+anchor)
source=source.replace('2026-09-07-eliminate-budget-main-ctest.log','2026-09-08-score-increment-main-ctest.log')
source=source.replace('20260907-2255-eliminate-budget','20260908-0857-score-increment')
source=source.replace('2026-09-07-eliminate-budget-validation.json','2026-09-08-score-increment-validation.json')
source=source.replace('seconds=5.32','seconds=6.21')
start,end=source.index('    source_status='),source.index('\n    manual_removed_bytes=')
source=source[:start]+"""    source_status='Production unchanged.5696 diagnostics-only source/tests89/89;5694/5698 full1M replay proves one extra increment generation,not extra admission.5704 both1200s timeouts/native cap,not validated;5710 corrects filter.5702 exact bounded memory pair and5687 full work/proof replayPASS;full5687 checks pending.5679wave1 and full5712 submitted. Goal unmet,no promotion.',
    fuzz_owner='5657/95829 terminalPASS5649:70000 cases,63573 checked verdicts,zero solver failures,312 reachability cases. Fixture-only GCC read workaround reported by owner. New5715/4746 owns5672;only CLI responses observed,no independent fuzz job/artifact inspection.',
""".rstrip()+source[end:]
source=source.replace("reviewer_cleanup='Claude removed only its newly generated 121-byte predecessor ctest -N log and new temporary directories; no immutable content or user data removed.',",
    "reviewer_cleanup='No manual deletion;unique sources/pins/traces retained.5704 timeout runner removed its own scratch;resource exits distinct from solver failures.',")
source=source.replace('result=dict(observed_at=', 'result=dict(fixed_authorities={},observed_at=')
anchor='checks={name:run('
assert source.count(anchor)==1
source=source.replace(anchor,"""fixture_checks={}
for script in ('compare-rank33-score-history-5695.py','compare-rank33-score-bumps-5699.py','compare-rank33-analysis-search-5711.py','validate-rank39-memory-5709.py'):
    p=root/'benchmarks/probes/scratch'/script
    fixture_checks[script]=dict(sha256=sha(p),output=run(['python3',str(p),'--self-test']))
"""+anchor)
source=source.replace('campaigns=campaigns,checks=checks,','campaigns=campaigns,checks=checks,fixture_checks=fixture_checks,')
exec(compile(source,__file__,'exec'))
