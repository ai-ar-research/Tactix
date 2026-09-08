#!/usr/bin/env python3
"""Retain all inherited proof gates and add the new policy refutations."""
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
here=Path(__file__).resolve().parent
c=root/'benchmarks/runs/20260908-5962-bve-active-pop'
source=here/'validate-forward-unit-5801.py'
code=source.read_text().replace('20260908-5798-forward-unit/fixed-proofs','20260908-5962-bve-active-pop/reduce-proofs')
code=code.replace('pin-5798-forward-unit-release/tests/test_forward_unit','pin-5962-bve-active-pop-release/tests/test_reduce_root_guidance')
code=code.replace('len(records) == 16','len(records) == 6').replace('checked_micro_proofs=16','checked_micro_proofs=6').replace('state_model_cases=17','state_model_cases=3')
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
source=here/'validate-reduce-root-final-5904.py'
code=source.read_text()
for old,new in [('20260908-5904-reduce-root-edges','20260908-5966-bve-active-pop-edges'),
                ('5903-reduce-root','5965-bve-active-pop'),('5900-reduce-root','5962-bve-active-pop'),
                ('runtime-reduce-root-5900','runtime-bve-active-pop-5962'),('against5900','against5962'),
                ('final-proof-validation.json','inherited-proof-validation.json')]:code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inherited=json.loads((c/'inherited-proof-validation.json').read_text())
own=json.loads((c/'controls/summary.json').read_text())
assert inherited['checked_proofs']==46 and own['checked_proofs']==8
(c/'final-proof-validation.json').write_text(json.dumps(dict(checked_proofs=54,
    hashes={str(p.relative_to(root)):sha(p) for p in [c/'inherited-proof-validation.json',c/'controls/summary.json']},
    scope='8 new inactive-pop policy refutations plus46 inherited Reduce/continuation/caller/edge/active-journal proofs.24 actual old/new SAT/model cases,6 native controls. No checker acceptance change or fuzz claim.'),indent=2)+'\n')
print('PASS:54 independently checked proofs')
