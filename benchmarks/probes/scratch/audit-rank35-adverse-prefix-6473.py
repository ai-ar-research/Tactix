#!/usr/bin/env python3
"""Seal the extended Rank35 closure, new diagnostic launches and timing waves."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank45-cache-6420.py')
code=source.read_text()
for old,new in {
 '2026-09-09-rank45-cache-audit-6420.json':'2026-09-09-rank35-adverse-prefix-audit-6473.json',
 '2026-09-09-vivify-count-cache-audit-6400.json':'2026-09-09-activation-full50-audit-6462.json',
 "owned={'PROGRESS.md','tools/bench/decision_prefix.py','tools/bench/test_decision_prefix.py',\n       'benchmarks/reports/2026-09-09-rank45-scheduling-and-cache-gates.md'}":"owned={'PROGRESS.md','benchmarks/reports/2026-09-09-rank35-three-million-and-rank33-prefix.md'}",
 'range(6402,6422)':'range(6467,6475)',
 "('6357-compact-stamp','6383-vivify-count-cache','6413-rank45-schedule')":"('6383-vivify-count-cache','6424-activation-stamp','6448-shared-blocker')",
 "'6410-rank45-transitive-census','6415-rank45-transitive-schedule')":"'6410-rank45-transitive-census','6415-rank45-transitive-schedule','6427-activation-stamp-BC-gate','6433-activation-rank45-prefix','6436-activation-rank32-prefix','6439-activation-ranks1-50','6451-shared-blocker-BC-gate','6459-rank35-later-prefix','6467-rank35-near-final-prefix','6470-rank33-activation-prefix','6443-activation-three-wave-runtime','6454-shared-blocker-three-wave-runtime','6464-activation-adverse-three-wave-runtime')",
 '20260909-0745-rank45-cache':'20260909-0930-rank35-and-adverse-prefix',
 '6404 failed a derived start-counter assumption, not a solver/proof oracle. Frozen failed validator preserved;6409 uses actual observer accounting with two maintained regression tests. Early incomplete-wave submission refused before writing or submitting, then wave1 submitted once when complete. Initial apply_patch context mismatch made no edits. No solver/proof/model/build failure or excluded measurement.':'No new solver/proof/model/build failure or excluded measurement.6461 successfully sealed the3M comparison;6469/6472 pending validations created no output. No source change. No resubmission or modification of a successful frozen artifact. Any PDF layout failure is retained in report-overflow.log before correction and two clean compiles.',
 'Continue exact6405 timing and6407 full50 outcomes without waiting for proof checks; validate6415 schedule replay before comparing keys. Preserve Rank32 closure and use general source-backed repairs only. No source promotion, fuzz inspection, denied transfer or push retry.':'Validate6467 current6424/6305 Rank35 continuation and6470 three-arm Rank33 prefix. Complete all three predeclared waves of6443,6454,6464; summarize each once after18 expected outcomes, without waiting for proof checks. Preserve fixed50 reference, full50 discrepancies, memory and fuzz gates. No source promotion, fuzz inspection or denied transfer/push retry.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)"
assert code.count(anchor)==1
code=code.replace(anchor,anchor+"\nfor rel,digest in prior['owned_hashes'].items():\n    if rel!='PROGRESS.md': assert sha(root/rel)==digest,rel")
anchor='commands=[(n,'; assert code.count(anchor)==1
extra="""
closure=root/'benchmarks/runs/20260909-6459-rank35-later-prefix/summary.json'
s=json.loads(closure.read_text())
assert s['stop']==3000000 and s['matching_decisions_from_start']==159573175
assert s['decisions']['complete_equal'] and s['validated_overlap_rows']==21
assert s['suffix_rows']=={'candidate':60375818,'previous':60375818}
assert s['prefixes']['candidate']['hash']==s['prefixes']['previous']['hash']=='4bfd01ec4333149f'
assert s['boundaries_search']['equal']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
retain(closure)
for name in ('6467-rank35-near-final-prefix','6470-rank33-activation-prefix'):
    c=root/('benchmarks/runs/20260909-'+name); m=json.loads((c/'manifest.json').read_text())
    for binding in m['authorities'].values(): assert sha(root/binding['path'])==binding['sha256']
    for key in ('prior_closure','prior_full_solve','selection','provisional_gate'):
        if key in m:
            binding=m[key]; assert sha(root/binding['path'])==binding['sha256']
    if name.startswith('6467'):
        assert m['stop_conflict']==4400000 and m['stream_from']==3000000 and m['common_decisions']==159573175
        assert m['prior_common_hash']=='4bfd01ec4333149f'
    else:
        assert m['rank']==33 and m['stop_conflict']==100000
for name,release,trace in (('6424-activation-stamp',108,107),('6448-shared-blocker',109,108)):
    c=root/f'benchmarks/runs/20260909-{name}-build'; b=json.loads((c/'manifest.json').read_text())
    for kind,count in (('release',release),('bc',release),('trace',trace)):
        assert f'100% tests passed out of {count}' in (c/'logs'/f'{kind}-ctest.log').read_text()
c=root/'benchmarks/runs/20260909-6448-shared-blocker-build'
control=json.loads((c/'control-6450.json').read_text())
assert control['outputs']['candidate']==control['outputs']['previous']
for group in ('hashes','logs'):
    for rel,digest in control[group].items(): assert sha(root/rel)==digest,rel
full=root/'benchmarks/reports/2026-09-09-activation-full50-6441/summary.json'
s=json.loads(full.read_text()); assert len(s['rows'])==50 and s['score']['expected_solves']==50
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
for p in full.parent.iterdir(): retain(p)
fresh=root/'benchmarks/reports/2026-09-09-fresh-rank50-final-6227/summary.json'
s=json.loads(fresh.read_text())
assert s['records']==s['final_records']==100 and s['all_certificates_checked'] and s['zero_residue']
local[str(fresh.relative_to(root))]=dict(bytes=fresh.stat().st_size,sha256=sha(fresh))
"""
code=code.replace(anchor,extra+'\n'+anchor)
anchor="assert 'Overfull' not in (report/'report.log').read_text()"
assert code.count(anchor)==1
code=code.replace(anchor,"if (report/'report-overflow.log').exists(): retain(report/'report-overflow.log')\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
