#!/usr/bin/env python3
"""Seal causal closure and timing evidence without waiting for certificates."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank45-cache-6420.py')
code=source.read_text()
changes={
 '2026-09-09-rank45-cache-audit-6420.json':'2026-09-09-activation-closure-audit-6446.json',
 '2026-09-09-vivify-count-cache-audit-6400.json':'2026-09-09-activation-stamp-audit-6431.json',
 "owned={'PROGRESS.md','tools/bench/decision_prefix.py','tools/bench/test_decision_prefix.py',\n       'benchmarks/reports/2026-09-09-rank45-scheduling-and-cache-gates.md'}":"owned={'PROGRESS.md','benchmarks/reports/2026-09-09-activation-closure-and-cache-timing.md'}",
 'range(6402,6422)':'range(6436,6448)',
 "('6357-compact-stamp','6383-vivify-count-cache','6413-rank45-schedule')":"('6357-compact-stamp','6383-vivify-count-cache','6424-activation-stamp')",
 "'6410-rank45-transitive-census','6415-rank45-transitive-schedule')":"'6410-rank45-transitive-census','6415-rank45-transitive-schedule','6427-activation-stamp-BC-gate','6433-activation-rank45-prefix','6436-activation-rank32-prefix')",
 '20260909-0745-rank45-cache':'20260909-0830-activation-closure',
 '6404 failed a derived start-counter assumption, not a solver/proof oracle. Frozen failed validator preserved;6409 uses actual observer accounting with two maintained regression tests. Early incomplete-wave submission refused before writing or submitting, then wave1 submitted once when complete. Initial apply_patch context mismatch made no edits. No solver/proof/model/build failure or excluded measurement.':'No new solver/proof/model failure or excluded measurement. Pending Rank32 observations did not create artifacts or cause resubmission. Cache6419 is a neutral timing result, not a failed gate. Historical failures remain frozen in prior audits.',
 'Continue exact6405 timing and6407 full50 outcomes without waiting for proof checks; validate6415 schedule replay before comparing keys. Preserve Rank32 closure and use general source-backed repairs only. No source promotion, fuzz inspection, denied transfer or push retry.':'Read PROGRESS for exact6439 full50 and6443 timing next actions. Never rerun successful6435/6438/6419/6429/6442 snapshots. Proofs remain asynchronous; fuzz, memory and promotion open. No denied transfer/push retry or fuzz inspection.',
}
for old,new in changes.items():
    assert old in code,old; code=code.replace(old,new)
anchor="assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)"
code=code.replace(anchor,anchor+"\nfor rel,digest in prior['owned_hashes'].items():\n    if rel!='PROGRESS.md': assert sha(root/rel)==digest,rel")
anchor='for name in names:'; assert code.count(anchor)==1
code=code.replace(anchor,"""for name in ('6439-activation-ranks1-50','6443-activation-three-wave-runtime'):
    c=root/('benchmarks/runs/20260909-'+name)
    if (c/'launch.json').exists() or list(c.glob('wave*-submission.json')): names+= (name,)
"""+anchor)
anchor='commands=[(n,'; assert code.count(anchor)==1
extra="""
for rel in (
    'benchmarks/runs/20260909-6427-activation-stamp-BC-gate/solver-checkpoint-6429.json',
    'benchmarks/runs/20260909-6427-activation-stamp-BC-gate/parent-work-checkpoint-6442.json',
    'benchmarks/runs/20260909-6433-activation-rank45-prefix/summary.json',
    'benchmarks/runs/20260909-6436-activation-rank32-prefix/summary.json',
    'benchmarks/runs/20260909-6405-count-cache-three-wave-runtime/solver-checkpoint-6419.json'):
    p=root/rel; s=json.loads(p.read_text()); retain(p)
    for q,digest in s.get('hashes',{}).items(): assert sha(root/q)==digest,q
b=json.loads((root/'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json').read_text())
for kind,count in (('release',108),('bc',108),('trace',107)):
    assert f'100% tests passed out of {count}' in (root/'benchmarks/runs/20260909-6424-activation-stamp-build/logs'/f'{kind}-ctest.log').read_text()
fresh=root/'benchmarks/reports/2026-09-09-fresh-rank50-final-6227/summary.json'
s=json.loads(fresh.read_text())
assert s['records']==s['final_records']==100 and s['all_certificates_checked'] and s['zero_residue']
local[str(fresh.relative_to(root))]=dict(bytes=fresh.stat().st_size,sha256=sha(fresh))
"""
code=code.replace(anchor,extra+'\n'+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
