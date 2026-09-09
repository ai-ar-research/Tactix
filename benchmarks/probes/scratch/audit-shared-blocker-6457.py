#!/usr/bin/env python3
"""Audit the isolated propagation experiment and ongoing activation measurement."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank45-cache-6420.py')
code=source.read_text()
for old,new in {
 '2026-09-09-rank45-cache-audit-6420.json':'2026-09-09-shared-blocker-audit-6457.json',
 '2026-09-09-vivify-count-cache-audit-6400.json':'2026-09-09-activation-closure-audit-6446.json',
 "owned={'PROGRESS.md','tools/bench/decision_prefix.py','tools/bench/test_decision_prefix.py',\n       'benchmarks/reports/2026-09-09-rank45-scheduling-and-cache-gates.md'}":"owned={'PROGRESS.md','benchmarks/reports/2026-09-09-shared-blocker-experiment.md'}",
 'range(6402,6422)':'range(6448,6459)',
 "('6357-compact-stamp','6383-vivify-count-cache','6413-rank45-schedule')":"('6383-vivify-count-cache','6424-activation-stamp','6448-shared-blocker')",
 "'6410-rank45-transitive-census','6415-rank45-transitive-schedule')":"'6410-rank45-transitive-census','6415-rank45-transitive-schedule','6427-activation-stamp-BC-gate','6433-activation-rank45-prefix','6436-activation-rank32-prefix','6439-activation-ranks1-50','6451-shared-blocker-BC-gate')",
 '20260909-0745-rank45-cache':'20260909-0845-shared-blocker',
 '6404 failed a derived start-counter assumption, not a solver/proof oracle. Frozen failed validator preserved;6409 uses actual observer accounting with two maintained regression tests. Early incomplete-wave submission refused before writing or submitting, then wave1 submitted once when complete. Initial apply_patch context mismatch made no edits. No solver/proof/model/build failure or excluded measurement.':'No new solver/proof/model/build failure or excluded measurement.6443 refused before creating output while its full-run witness oracles were pending. Initial PDF had3.23338pt overflow, caught before the audit wrote its checkpoint. Original log retained; paragraph reflowed and two clean compiles required. No resubmission or changed frozen artifact. All source suites and actual old/new fixture controls pass.',
 'Continue exact6405 timing and6407 full50 outcomes without waiting for proof checks; validate6415 schedule replay before comparing keys. Preserve Rank32 closure and use general source-backed repairs only. No source promotion, fuzz inspection, denied transfer or push retry.':'Complete full6439 measurement and activation timing6443;6453 requires all66 shared-blockerBC expected outcomes and33exactparentwork/proof pairs, then6454/6455 three-wave cost experiment. No new native refresh, source promotion, fuzz inspection, denied transfer or push retry.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)"
code=code.replace(anchor,anchor+"\nfor rel,digest in prior['owned_hashes'].items():\n    if rel!='PROGRESS.md': assert sha(root/rel)==digest,rel")
anchor='for name in names:'; assert code.count(anchor)==1
code=code.replace(anchor,"""for name in ('6443-activation-three-wave-runtime','6454-shared-blocker-three-wave-runtime'):
    c=root/('benchmarks/runs/20260909-'+name)
    if list(c.glob('wave*-submission.json')): names+=(name,)
"""+anchor)
anchor='commands=[(n,'; assert code.count(anchor)==1
extra="""
c=root/'benchmarks/runs/20260909-6448-shared-blocker-build'
b=json.loads((c/'manifest.json').read_text())
for rel,digest in b['patches'].items(): assert sha(c/rel)==digest
for kind,count in (('release',109),('bc',109),('trace',108)):
    assert f'100% tests passed out of {count}' in (c/'logs'/f'{kind}-ctest.log').read_text()
control=json.loads((c/'control-6450.json').read_text())
assert control['outputs']['candidate']==control['outputs']['previous']
for group in ('hashes','logs'):
    for rel,digest in control[group].items(): assert sha(root/rel)==digest,rel
for p in c.iterdir():
    if p.suffix in ('.json','.patch'): retain(p)
for p in (c/'logs').iterdir(): retain(p)
fresh=root/'benchmarks/reports/2026-09-09-fresh-rank50-final-6227/summary.json'
s=json.loads(fresh.read_text())
assert s['records']==s['final_records']==100 and s['all_certificates_checked'] and s['zero_residue']
local[str(fresh.relative_to(root))]=dict(bytes=fresh.stat().st_size,sha256=sha(fresh))
for p in (root/'benchmarks/reports/2026-09-09-activation-full50-6441').glob('*'): retain(p)
"""
code=code.replace(anchor,extra+'\n'+anchor)
anchor="assert 'Overfull' not in (report/'report.log').read_text()"
assert code.count(anchor)==1
code=code.replace(anchor,"retain(report/'report-overflow.log')\n"+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
