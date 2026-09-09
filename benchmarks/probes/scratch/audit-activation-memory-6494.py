#!/usr/bin/env python3
"""Seal the extended Rank35 closure, new diagnostic launches and timing waves."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank45-cache-6420.py')
code=source.read_text()
for old,new in {
 '2026-09-09-rank45-cache-audit-6420.json':'2026-09-09-activation-memory-audit-6494.json',
 '2026-09-09-vivify-count-cache-audit-6400.json':'2026-09-09-late-prefix-audit-6489.json',
 "owned={'PROGRESS.md','tools/bench/decision_prefix.py','tools/bench/test_decision_prefix.py',\n       'benchmarks/reports/2026-09-09-rank45-scheduling-and-cache-gates.md'}":"owned={'PROGRESS.md','benchmarks/reports/2026-09-09-activation-replicated-and-memory-gate.md'}",
 'range(6402,6422)':'range(6491,6496)',
 "('6357-compact-stamp','6383-vivify-count-cache','6413-rank45-schedule')":"('6383-vivify-count-cache','6424-activation-stamp','6448-shared-blocker','6475-position-key')",
 "'6410-rank45-transitive-census','6415-rank45-transitive-schedule')":"'6410-rank45-transitive-census','6415-rank45-transitive-schedule','6427-activation-stamp-BC-gate','6433-activation-rank45-prefix','6436-activation-rank32-prefix','6439-activation-ranks1-50','6451-shared-blocker-BC-gate','6459-rank35-later-prefix','6467-rank35-near-final-prefix','6470-rank33-activation-prefix','6443-activation-three-wave-runtime','6454-shared-blocker-three-wave-runtime','6464-activation-adverse-three-wave-runtime','6478-position-key-BC-gate','6483-rank35-vivify-replay','6486-rank33-later-prefix','6491-position-key-three-wave-runtime')",
 '20260909-0745-rank45-cache':'20260909-1020-activation-memory',
 '6404 failed a derived start-counter assumption, not a solver/proof oracle. Frozen failed validator preserved;6409 uses actual observer accounting with two maintained regression tests. Early incomplete-wave submission refused before writing or submitting, then wave1 submitted once when complete. Initial apply_patch context mismatch made no edits. No solver/proof/model/build failure or excluded measurement.':'No new solver/proof/model/build failure or excluded measurement.6445 and6466 sealed all18 expected outcomes each;6480 sealed66 expected and33exact6424 work/proof pairs. New paired-memory reader passes7 fixed tests. No solver/observer/checker source change, successful artifact rerun, job resubmission or frozen-source edit. Preparation escaping was corrected during inspection before execution. No new PDF overflow. First audit failed before writing at bounded stop_conflict lookup on runtime6491; fixed only diagnostic-loop membership and reran all checks.6456 then sealed all18 exact-parent outcomes; shared-blocker rejected for no gain and4.3percent Rank45 regression.',
 'Continue exact6405 timing and6407 full50 outcomes without waiting for proof checks; validate6415 schedule replay before comparing keys. Preserve Rank32 closure and use general source-backed repairs only. No source promotion, fuzz inspection, denied transfer or push retry.':'6445/6466/6480 DONE frozen: do not rerun. Activation Rank45 benefit and17/33/43 regressions replicated; retain no aggregate improvement claim. Complete6491 position-key memory waves via6492 after preceding six expected outcomes, not proof checks, then6493 afterall18. Keep separate from6448. Validate6483 via6485 and6486 via6488 with exact own replay/overlap first.6456 DONE frozen all18; reject6448 shared-blocker as a convergence improvement, do not compose or repeat. Preserve fixed50, full50 discrepancies, memory/fuzz gates; no promotion, fuzz inspection or denied transfer/push retry.',
}.items():
    assert old in code,old; code=code.replace(old,new)
anchor="assert not subprocess.check_output(['git','diff','--','src','include','CMakeLists.txt'],cwd=root)"
assert code.count(anchor)==1
code=code.replace(anchor,anchor+"\nfor rel,digest in prior['owned_hashes'].items():\n    if rel!='PROGRESS.md': assert sha(root/rel)==digest,rel")
anchor='commands=[(n,'; assert code.count(anchor)==1
extra="""
c=root/'benchmarks/runs/20260909-6475-position-key-build'
b=json.loads((c/'manifest.json').read_text())
for rel,digest in b['patches'].items(): assert sha(c/rel)==digest
controls=json.loads((c/'control-6477.json').read_text())
old,new=controls['measurements']['previous'],controls['measurements']['candidate']
for key in ('cases','sorted_cases','sorted_refs','digest'): assert old[key]==new[key]
assert old['cases']==480 and old['sorted_refs']==52714
assert old['allocated_bytes']==1054280 and new['allocated_bytes']==632568
assert new['digest']=='8aa3bddc2f6c1ed7'
for group in ('hashes','logs'):
    for rel,digest in controls[group].items(): assert sha(root/rel)==digest,rel
for p in c.iterdir():
    if p.suffix in ('.json','.patch'): retain(p)
for p in (c/'logs').iterdir(): retain(p)
owned.update({'tools/bench/vivify_trace.py','tools/bench/test_vivify_trace.py'})
for name,rank,stop,count in (('6467-rank35-near-final-prefix',35,4400000,161899386),('6470-rank33-activation-prefix',33,100000,31494584)):
    p=root/('benchmarks/runs/20260909-'+name+'/summary.json'); s=json.loads(p.read_text())
    assert s['rank']==rank and s['stop']==stop
    if rank==35:
        assert s['matching_decisions_from_start']==count and s['validated_overlap_rows']==1
        assert s['decisions']['left']==[3074183,161631566,1122,-482577,0]
        assert s['decisions']['right']==[3074183,161631566,1122,-486307,0]
    else:
        assert all(pair['complete_equal'] and pair['matching_rows']==count for pair in s['pairs'].values())
    for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
    retain(p)
for name in ('6483-rank35-vivify-replay','6486-rank33-later-prefix'):
    c=root/('benchmarks/runs/20260909-'+name); m=json.loads((c/'manifest.json').read_text())
    for binding in m['authorities'].values(): assert sha(root/binding['path'])==binding['sha256']
    for key in ('parent_summary','prior_closure','selection','provisional_gate'):
        if key in m:
            binding=m[key]; assert sha(root/binding['path'])==binding['sha256']
    if name.startswith('6483'):
        assert m['diagnostic_conflict']==3069160 and m['stop_conflict']==4400000
        a,b=m['selected_vivify_invocations']['candidate'],m['selected_vivify_invocations']['previous']
        assert len(a)==len(b)==2 and a[1]['propagations']-b[1]['propagations']==1 and a[1]['ticks']-b[1]['ticks']==52
    else:
        assert m['stop_conflict']==1000000 and m['common_decisions']==31494584 and m['prior_common_hash']=='5c263e00aa882466'
owned.update({'tools/bench/paired_memory.py','tools/bench/test_paired_memory.py'})
for name,number,ranks in (('6443-activation-three-wave-runtime',6445,{32,35,45}),('6464-activation-adverse-three-wave-runtime',6466,{17,33,43}),('6454-shared-blocker-three-wave-runtime',6456,{32,35,45})):
    p=root/f'benchmarks/runs/20260909-{name}/solver-checkpoint-{number}.json'
    s=json.loads(p.read_text())
    assert s['solver_outcomes']==18 and s['exact_per_arm_work_and_proof']
    assert set(map(int,s['ranks']))==ranks
    assert all(v['absolute_range_within_two_percent'] for v in s['ranks'].values())
    for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
    retain(p)
p=root/'benchmarks/runs/20260909-6478-position-key-BC-gate/solver-checkpoint-6480.json'
s=json.loads(p.read_text()); assert s['solver_records']==66 and s['exact_parent_candidate_work_and_proof']
assert len(s['parent_oracles'])==33
for group in ('hashes','parent_oracles'):
    for rel,digest in s[group].items(): assert sha(root/rel)==digest,rel
retain(p)
c=root/'benchmarks/runs/20260909-6491-position-key-three-wave-runtime'
m=json.loads((c/'manifest.json').read_text())
assert set(m['witnesses'])=={'10','25','47'}
assert m['work_oracles']['candidate']==m['work_oracles']['previous']
assert m['resource_contract']['cpus_per_task']==12 and m['resource_contract']['memory_mib']==48000
for key in ('complete50_selection','provisional_gate'):
    binding=m[key]; assert sha(root/binding['path'])==binding['sha256']
for binding in m['authorities'].values(): assert sha(root/binding['path'])==binding['sha256']
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
for name,release,trace in (('6424-activation-stamp',108,107),('6448-shared-blocker',109,108),('6475-position-key',109,108)):
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
