#!/usr/bin/env python3
"""Predeclare matched memory/runtime evidence after the complete exact-work BC gate."""
import json
from pathlib import Path
source=Path(__file__).with_name('prepare-collapse-timing-6332.py')
root=source.resolve().parents[3]
gate=root/'benchmarks/runs/20260909-6478-position-key-BC-gate/solver-checkpoint-6480.json'
full=root/'benchmarks/runs/20260909-6439-activation-ranks1-50'
if not gate.exists():
    print('Complete exact-work6480 B/C gate pending; no output created'); raise SystemExit(0)
g=json.loads(gate.read_text())
assert g['solver_records']==66 and g['exact_parent_candidate_work_and_proof']
for rank in (10,25,47):
    name=f'{rank-1:03}.json'
    if not (full/'results'/name).exists() and not (full/'provisional'/name).exists():
        print('Parent full-run10/25/47 oracles pending; no output created'); raise SystemExit(0)
code=source.read_text()
for old,new in {
 '20260909-6332-collapse-three-wave-runtime':'20260909-6491-position-key-three-wave-runtime',
 '20260909-6311-substitute-collapse-BC-gate':'20260909-6478-position-key-BC-gate',
 "('candidate',6305,'substitute-collapse','20260909-6320-substitute-collapse-ranks1-50')":"('candidate',6475,'position-key','20260909-6439-activation-ranks1-50')",
 "('previous',6269,'vivify-binary-retry','20260909-6278-vivify-binary-ranks1-50')":"('previous',6424,'activation-stamp','20260909-6439-activation-ranks1-50')",
 '(32,33,40)':'(10,25,47)',
 '6332-collapse-timing':'6491-position-key-memory',
 'tactix-collapse-changed-work-three-waves-v1':'tactix-position-key-memory-three-waves-v1',
 "'32':'primary Rank32 search reduction','33':'second productive changed-work family','40':'observed adverse search-work control'":"'10':'highest full50 absolute peak RSS','25':'large retained-memory excess versus Kissat','47':'highest peak RSS among added41-50'",
 '6305 retained-endpoint retry causes repeatable changed-work runtime effects versus6269, including the adverseRank40 case.':'Narrowing two position-sort key buffers may lower whole-solver memory or execution cost while preserving every6424 work/proof byte. Primary memory metric is full-solver peak RSS across all three pairs; sampled VmPeak is separate. No whole-process saving assumed from allocation controls, no selective extra waves. Runtime requires all three pairs and <=2% per-arm spread for absolute claims.',
 'Unchanged pinned runner/oracles; existing39 benchmark13watcher tests. Matrix validates18tasks,pairedarmcoverage; source gates105/104,72fixtures,8proofs alreadypassed. New timing compares different search work,not isolated instruction cost.':'Unchanged immutable runtime runner; source109Release twice108trace pass;480 actual old/new ordering fixtures and exact40percent sort-allocation reduction. All66BC expected and33candidate exact6424 work/proof pairs prerequisite. New paired-memory summarizer has7 fixed tests for pairing, complete coverage, unknown samples and invalid data. All18 candidate/parent tasks retain exact work/proof oracles.',
}.items():
    assert old in code,old; code=code.replace(old,new)
marker="m['pin_sha256']="; assert code.count(marker)==1
code=code.replace(marker,"m['provisional_gate']={'path':str(checkpoint.relative_to(root)),'sha256':sha(checkpoint)}\nassert oracles['candidate']==oracles['previous']\n"+marker)
code=code.replace("('pending_validation','checked_UNSAT')","('pending_validation','checked_UNSAT','checked_SAT')")
code=code.replace("r['expected']=='UNSAT'","r['expected'] in ('SAT','UNSAT')")
selection=root/'benchmarks/reports/2026-09-09-activation-full50-6441/summary.json'
table=json.loads(selection.read_text())['rows']
assert max(table,key=lambda r:r['candidate_peak_rss_kib'])['rank']==10
assert max((r for r in table if r['rank']>=41),key=lambda r:r['candidate_peak_rss_kib'])['rank']==47
row25=next(r for r in table if r['rank']==25)
assert row25['candidate_peak_rss_kib']==3408252 and row25['rss_over_kissat']>3.8
code=code.replace(marker,"m['complete50_selection']={'path':str(selection.relative_to(root)),'sha256':sha(selection)}\n"+marker)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','checkpoint':gate,'selection':selection})
