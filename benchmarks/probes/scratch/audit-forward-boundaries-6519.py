#!/usr/bin/env python3
"""Seal aligned forward boundaries and current asynchronous certificate state."""
from pathlib import Path
import re
source=Path(__file__).with_name('audit-congruence-stages-6513.py')
wrapper=source.read_text()
end="\nexec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})\n"
assert wrapper.count(end)==1
namespace={'__file__':str(source),'__name__':'assemble_only'}
exec(compile(wrapper.replace(end,'\nassembled=code\n'),str(source),'exec'),namespace)
code=namespace['assembled']
for old,new in {
 '2026-09-09-congruence-stages-audit-6513.json':'2026-09-09-forward-boundaries-audit-6519.json',
 '2026-09-09-rank33-census-audit-6505.json':'2026-09-09-congruence-stages-audit-6513.json',
 "owned={'PROGRESS.md','benchmarks/reports/2026-09-09-congruence-installed-and-input.md','tools/bench/congruence_stages.py','tools/bench/test_congruence_stages.py'}":"owned={'PROGRESS.md','benchmarks/reports/2026-09-09-forward-boundary-localization.md','tools/bench/binary_snapshot.py','tools/bench/test_binary_snapshot.py'}",
 'range(6508,6516)':'range(6516,6527)',
 "'6501-rank33-congruence-replay','6510-rank33-congruence-stages')":"'6501-rank33-congruence-replay','6510-rank33-congruence-stages','6516-forward-before','6516-forward-after','6522-equivalence-life')",
 '20260909-1105-congruence-stages':'20260909-1120-forward-boundaries',
}.items():
    assert code.count(old)==1,(old,code.count(old)); code=code.replace(old,new)
code,n=re.subn(r"failure_classification='[^']*'",
    "failure_classification='No build, unit-test, solver, proof or model failure; no excluded measurement, successful frozen snapshot rerun, production source change, fuzz inspection or denied transfer/push retry.'",code)
assert n==1
code,n=re.subn(r"next='[^']*'",
    "next='6518/6521/6524 localize extra91609=91779 to XOR matching with a root-false padded gate;6526 fixed public controls/proofs pass. Retain stronger behavior, not native counting omission. Return to Rank32 unit-cost decomposition using existing checked profiles and exact full-work authorities before choosing another repair. Rank33 full runtime loss remains scored/unexplained. Keep all50/memory, fixed references, production, pending checkers and rejected/neutral decisions.'",code)
assert n==1
anchor='commands=[(n,'; assert code.count(anchor)==1
extra="""
p=root/'benchmarks/runs/20260909-6516-forward-before/comparison-6518.json'
s=json.loads(p.read_text()); assert s['parent_replay'] and s['cross_campaign_binary_snapshots_exact']
assert len(s['binary_comparisons'])==4 and set(s['first_call_large'])=={'before','after'}
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
retain(p)
q=p.parent/'equivalence-input-6521.json'; z=json.loads(q.read_text())
assert z['parent_summary_sha256']==sha(p)
assert z['duplicate_copies_of_native_pairs']==18 and len(z['shared_direct_xor_witnesses'])==1
retain(q)
q=root/'benchmarks/runs/20260909-6526-padded-xor-control/summary.json'
z=json.loads(q.read_text()); assert z['cases']==16 and z['final_proofs_checked']==8
for rel,digest in z['hashes'].items(): assert sha(root/rel)==digest,rel
assert sha(q.parent/'fixture')==z['fixture_binary_sha256']
assert len(list(q.parent.glob('*.cnf')))==len(list(q.parent.glob('*.proof')))==0
for p in q.parent.iterdir():
    if p.suffix in ('.json','.log'): retain(p)
retain(root/'benchmarks/probes/scratch/test-padded-xor-6525.cpp')
for rel in ('tools/solvers/variants/kissat-macro-heap-6090/src/congruence.c','tools/solvers/variants/kissat-macro-heap-6090/src/options.h'):
    p=root/rel; local[rel]=dict(bytes=p.stat().st_size,sha256=sha(p))
q=root/'benchmarks/runs/20260909-6439-activation-ranks1-50/results/032.json'
z=json.loads(q.read_text()); assert z['rank']==33 and z['valid'] and z['proof_check']['result']=='verified'
assert z['artifact_deleted'] and not z['cleanup_pending']; retain(q)
owned.update({'tools/bench/binary_lifecycle.py','tools/bench/test_binary_lifecycle.py'})
q=root/'benchmarks/runs/20260909-6522-equivalence-life/summary.json'
z=json.loads(q.read_text()); assert z['parent_replay']
for rel,digest in z['hashes'].items(): assert sha(root/rel)==digest,rel
retain(q)
for phase in ('before','after'):
    c=root/f'benchmarks/runs/20260909-6516-forward-{phase}'
    m=json.loads((c/'manifest.json').read_text())
    assert m['formula_ordinal']==(phase=='after') and m['phase']==phase
    assert m['resource_contract']['array']=='0-1'
    assert m['resource_contract']['cpus_per_task']==12 and m['resource_contract']['memory_mib']==48000
    for binding in m['authorities'].values(): assert sha(root/binding['path'])==binding['sha256']
"""
code=code.replace(anchor,extra+'\n'+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
