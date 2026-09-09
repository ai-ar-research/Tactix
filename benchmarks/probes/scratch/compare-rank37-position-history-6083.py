#!/usr/bin/env python3
"""Full replay before position-history bracketing; retain all unmatched sites."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(8<<20),b''):
            h.update(block)
    return h.hexdigest()
def validate_replay(c):
    m=json.loads((c/'manifest.json').read_text())
    parent=root/m['parent_campaign']
    assert sha(parent/'choice-validation.json')==m['parent_summary_sha256']
    for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest,rel
    out={}
    for arm in ('candidate','kissat'):
        p=c/'results'/f'{arm}.json';r=json.loads(p.read_text())
        q=parent/'results'/f'{arm}.json';old=json.loads(q.read_text())
        assert r['exit_code']==0 and r['scratch_deleted']
        assert r['binary_sha256']==old['binary_sha256']==m['hashes']['pins/'+arm]
        assert r['partial_certificate']==old['partial_certificate']
        for name,digest in r['trace_sha256'].items(): assert sha(c/'traces'/name)==digest,name
        for name,digest in old['trace_sha256'].items():
            assert sha(parent/'traces'/name)==digest==r['trace_sha256'][name],name
        out[arm]=dict(result_sha256=sha(p),parent_result_sha256=sha(q),traces=r['trace_sha256'])
    return out
source=Path(__file__).with_name('compare-rank33-score-history-5695.py')
code=source.read_text()
for old,new in {
    '20260908-5694-rank33-score-history':'20260908-6081-rank37-position-history',
    '120438':'116362','120439':'116363','182093':'272979','5694 pending':'6081 pending',
    "evidence=support['validate'](c)":"evidence=validate_replay(c)",
    "t[key]['score']!=k[key]['score']":"t[key]['position']!=k[key]['position']",
    'first_score_difference':'first_position_difference','unequal_common_score_count':'unequal_common_position_count',
}.items():
    assert old in code,old
    code=code.replace(old,new)
anchor="    p=c/'summary.json';assert not p.exists()"
assert code.count(anchor)==1
code=code.replace(anchor,"""    result['first_field_differences'] = {
        field: next((pair(key) for key in common if t[key][field] != k[key][field]), None)
        for field in ('score', 'position', 'value', 'active')}
    result['limitation'] += ' Equal scores at the last-common snapshot do not exclude historical score differences; report first differences in each field. The conflict cutoff can include post-split rows at272979,which are consequences rather than earlier causes.'
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__','validate_replay':validate_replay})
