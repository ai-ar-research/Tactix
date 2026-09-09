#!/usr/bin/env python3
"""Read-only structural witnesses for the selected equivalence; no SAT solve."""
from collections import Counter,defaultdict
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from congruence_stages import read_formula
from binary_snapshot import read_snapshots
c=root/'benchmarks/runs/20260909-6516-forward-before'
target=c/'equivalence-input-6521.json'; assert not target.exists()
p=c/'comparison-6518.json'; s=json.loads(p.read_text()); assert s['parent_replay']
assert s['first_call_large']['before']['identities_and_values']['equal_multiset']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
bounds=(('slot_boundary','name=congruence-dense-resume|stage=second-entry'),('slot_boundary','name=congruence-dense-resume|stage=second'))
with (c/'traces/candidate.forward-binaries.tsv').open() as f: t=read_snapshots(f,'tactix',227617,bounds)
bounds=(('congruence_dense_second_entry','-'),('congruence_dense_second','-'))
with (c/'traces/kissat.forward-binaries.tsv').open() as f: k=read_snapshots(f,'kissat',227617,bounds)
a,b=Counter(t[0]['rows']),Counter(k[0]['rows']); extra=a-b
new={pair:n for pair,n in extra.items() if pair not in b}
duplicates={pair:n for pair,n in extra.items() if pair in b}
assert sum(extra.values())==22 and new=={(-91779,91609):2,(-91609,91779):2}
gate_groups={lhs:defaultdict(dict) for lhs in (91609,91779)}
with (c/'traces/candidate.formula.tsv').open() as f:
    for identity,pose,values in read_formula(f,'tactix'):
        if identity[0] or 1 in values: continue
        opened=tuple(lit for lit,value in zip(identity[1],values) if value==0)
        if len(opened)!=3 or len(set(map(abs,opened)))!=3: continue
        for lhs,groups in gate_groups.items():
            if lhs not in map(abs,opened): continue
            rhs=tuple(sorted(abs(lit) for lit in opened if abs(lit)!=lhs))
            groups[rhs][opened]=dict(identity=identity,pose=pose,values=values)
complete={}
for lhs,groups in gate_groups.items():
    complete[lhs]={}
    for rhs,clauses in groups.items():
        for parity in (0,1):
            selected={key:row for key,row in clauses.items() if sum(lit<0 for lit in key)%2==parity}
            if len(selected)==4: complete[lhs][rhs,parity]=selected
shared=[]
for key in sorted(complete[91609].keys() & complete[91779].keys()):
    rhs,parity=key
    shared.append(dict(inputs=rhs,forbidden_parity=parity,
        clauses={str(lhs):[dict(open_clause=k,**v) for k,v in sorted(complete[lhs][key].items())] for lhs in (91609,91779)}))
out=dict(parent_summary_sha256=sha(p),first_precleanup_extra_binary_copies=sum(extra.values()),
    duplicate_copies_of_native_pairs=sum(duplicates.values()),new_pairs=list(sorted(new.items())),
    complete_direct_xor_groups={str(lhs):len(groups) for lhs,groups in complete.items()},
    shared_direct_xor_witnesses=shared,
    limitation='Structural four-sign-pattern census under observed pre-cleanup root values; no solver invocation or proof check. A shared pattern is a conditional local XOR witness, not evidence of extraction timing or actual producer. No matching pattern does not refute an equivalence obtained through representative closure.')
target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(dict(extra_copies=sum(extra.values()),duplicate_copies=sum(duplicates.values()),new_pairs=out['new_pairs'],
    direct_xor_groups=out['complete_direct_xor_groups'],shared_witnesses=len(shared)),indent=2))
