#!/usr/bin/env python3
"""Extend the previous exact campaign audit with the isolated activation repair."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank45-cache-6420.py')
code=source.read_text()
for old,new in {
 "2026-09-09-rank45-cache-audit-6420.json":"2026-09-09-activation-stamp-audit-6431.json",
 "2026-09-09-vivify-count-cache-audit-6400.json":"2026-09-09-rank45-cache-audit-6420.json",
 "owned={'PROGRESS.md','tools/bench/decision_prefix.py','tools/bench/test_decision_prefix.py',\n       'benchmarks/reports/2026-09-09-rank45-scheduling-and-cache-gates.md'}":"owned={'PROGRESS.md','benchmarks/reports/2026-09-09-activation-stamp-producer.md'}",
 "range(6402,6422)":"range(6422,6436)",
 "glob('*'+str(n)+'.py')":"glob('*'+str(n)+'.*')",
 "('6357-compact-stamp','6383-vivify-count-cache','6413-rank45-schedule')":"('6357-compact-stamp','6383-vivify-count-cache','6413-rank45-schedule','6424-activation-stamp')",
 "'6410-rank45-transitive-census','6415-rank45-transitive-schedule')":"'6410-rank45-transitive-census','6415-rank45-transitive-schedule','6427-activation-stamp-BC-gate')",
 "20260909-0745-rank45-cache":"20260909-0800-activation-stamp",
 "6404 failed a derived start-counter assumption, not a solver/proof oracle. Frozen failed validator preserved;6409 uses actual observer accounting with two maintained regression tests. Early incomplete-wave submission refused before writing or submitting, then wave1 submitted once when complete. Initial apply_patch context mismatch made no edits. No solver/proof/model/build failure or excluded measurement.":"Native fixed fixture6422 compilation failed because its IDX macro asserts through a variable named solver, while the fixture used s. Failed source/log retained; corrected successor6423 uses solver and passes all controls. First watcher invocation lacked required --job; corrected watcher observes same1677833, no resubmission. Initial Andrew report had26.88pt overflow; original log retained, paragraph spacing corrected before delivery and two clean compiles required. No solver/proof/model failure or excluded measurement; source full builds pass.",
 "Continue exact6405 timing and6407 full50 outcomes without waiting for proof checks; validate6415 schedule replay before comparing keys. Preserve Rank32 closure and use general source-backed repairs only. No source promotion, fuzz inspection, denied transfer or push retry.":"6429 gates all66 new6427 expected outcomes before6424 targetedD.6417 validates6415 before key interpretation; then Rank45 current6424 prefix and Rank32 preserved3M closure. Continue6405 finalwave after12exact outcomes and6418 full50 snapshot after50; never rerun successful artifact writers. Allsourcefuzz/memory/promotion remain open; no denied transfer/push retry or fuzz inspection.",
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="names=('6320-substitute-collapse-ranks1-50'"; assert code.count(anchor)==1
extra='''
c=root/'benchmarks/runs/20260909-6424-activation-stamp-build'
b=json.loads((c/'manifest.json').read_text())
for rel,digest in b['patches'].items(): assert sha(c/rel)==digest
for kind,count in (('release',108),('bc',108),('trace',107)):
    assert f'100% tests passed out of {count}' in (c/'logs'/f'{kind}-ctest.log').read_text()
for p in c.iterdir():
    if p.suffix in ('.json','.patch'): retain(p)
for p in (c/'logs').iterdir(): retain(p)
for name in ('6422-activation-stamp-control','6423-activation-stamp-control','6426-activation-stamp-controls','6430-unit-first-stamp-control'):
    c=root/('benchmarks/runs/20260909-'+name)
    for p in c.iterdir():
        if p.suffix in ('.json','.log'): retain(p)
control=json.loads((root/'benchmarks/runs/20260909-6426-activation-stamp-controls/summary.json').read_text())
assert control['parent_misses']==64 and control['native_matches']==16
for rel,digest in control['hashes'].items(): assert sha(root/rel)==digest
units=json.loads((root/'benchmarks/runs/20260909-6430-unit-first-stamp-control/summary.json').read_text())
assert units['outputs']['candidate']=='unit-first observations=16 missing=0\\n'
for rel,digest in units['hashes'].items(): assert sha(root/rel)==digest
fresh=root/'benchmarks/reports/2026-09-09-fresh-rank50-final-6227/summary.json'
s=json.loads(fresh.read_text())
assert s['records']==s['final_records']==100 and s['all_certificates_checked'] and s['zero_residue']
assert s['all_classified_for_scoring'] and not s['residue_tasks'] and not s['cleanup_unverified_tasks']
local[str(fresh.relative_to(root))]=dict(bytes=fresh.stat().st_size,sha256=sha(fresh))
'''
code=code.replace(anchor,extra+'\n'+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
