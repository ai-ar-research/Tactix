#!/usr/bin/env python3
"""Retain the inherited proof gates on the independently repaired policy."""
import hashlib
import json
import os
from pathlib import Path
root=Path(__file__).resolve().parents[3]
here=Path(__file__).resolve().parent
c=root / 'benchmarks/runs/20260908-5900-reduce-root'
for folder,pin,test,count in [('continuation-proofs','5900-reduce-root-release','forward_unit',16),
                              ('caller-proofs','5900-reduce-root-release','forward_caller',6),
                              ('journal-proofs','5903-reduce-root-journal','forward_caller',6)]:
    script=here / 'validate-forward-unit-5801.py'
    code=script.read_text().replace('20260908-5798-forward-unit/fixed-proofs','20260908-5900-reduce-root/'+folder)
    code=code.replace('pin-5798-forward-unit-release/tests/test_forward_unit',f'pin-{pin}/tests/test_{test}')
    code=code.replace('len(records) == 16',f'len(records) == {count}').replace('checked_micro_proofs=16',f'checked_micro_proofs={count}')
    code=code.replace('state_model_cases=17',f'state_model_cases={17 if count==16 else 3}')
    code=code.replace('17 state/model cases, 8 DRAT and 8 LRAT independently verified proofs',f'{folder}: {count} independent proofs')
    keys=['TACTIX_RULE_JOURNAL','TACTIX_RULE_JOURNAL_RULE','TACTIX_RULE_JOURNAL_CANDIDATE']
    assert not any(k in os.environ for k in keys)
    if folder=='journal-proofs':
        os.environ.update(TACTIX_RULE_JOURNAL=str(c / 'caller-journal.tsv'),TACTIX_RULE_JOURNAL_RULE='Propagate',TACTIX_RULE_JOURNAL_CANDIDATE='post-forward-connected')
    try: exec(compile(code,str(script),'exec'),{'__file__':str(script),'__name__':'__main__'})
    finally:
        if folder=='journal-proofs':
            for k in keys: del os.environ[k]
script=here / 'validate-forward-unit-edges-5813.py'
code=script.read_text()
for old,new in {'20260908-5813-forward-unit-edges':'20260908-5904-reduce-root-edges',
                'pin-5798-forward-unit-release':'pin-5900-reduce-root-release',
                'runtime-forward-unit-5798':'runtime-reduce-root-5900','against5798':'against5900'}.items():
    assert old in code
    code=code.replace(old,new)
exec(compile(code,str(script),'exec'),{'__file__':str(script),'__name__':'__main__'})
journal=c / 'caller-journal.tsv'
rows=[line.split('\t') for line in journal.read_text().splitlines() if not line.startswith('#')]
assert [r[14] for r in rows] == ['applied','applied','declined']+['refuted']*6
summaries=[c/(name+'/summary.json') for name in ('reduce-proofs','continuation-proofs','caller-proofs','journal-proofs','controls')]
summaries += [root / 'benchmarks/runs/20260908-5904-reduce-root-edges/summary.json',journal]
target=c/'final-proof-validation.json'
assert not target.exists()
target.write_text(json.dumps(dict(checked_proofs=46,journal_active_outcomes=[r[14] for r in rows],
    hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in summaries},
    scope='6 new Reduce-guidance proofs plus40 inherited continuation/caller/edge/active-journal proofs. Actual old/new/native guidance and original SAT model controls. No checker acceptance change.'),indent=2)+'\n')
print('PASS:46 independent proofs;all inherited active-journal outcomes exact')
