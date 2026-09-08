#!/usr/bin/env python3
"""Accept only the additional stages emitted by the pinned observer producers."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank39-memory-5688.py').read_text().split('\ndef sha(p):')[0]
anchor="fields[1] in ('begin','end')"
assert source.count(anchor)==1
source=source.replace(anchor,"""(fields[1] in ('begin','end') or
        (fields[0]=='Factor' and fields[1] in ('admitted','applied')) or
        (fields[0]=='congruence-dense-resume' and fields[1] in ('entry','first','second-entry','second')) or
        (fields[0]=='vivify-dense-resume' and fields[1] in ('entry','resume')))""")
source=source.replace('def fixtures():','def base_fixtures():')
exec(compile(source,__file__,'exec'))
def fixtures():
    base_fixtures()
    suffix='\t'+'\t'.join(k+'=1' for k in keys)+'\n'
    positive=[('Factor',s) for s in ('admitted','applied')]+[
        ('congruence-dense-resume',s) for s in ('entry','first','second-entry','second')]+[
        ('vivify-dense-resume',s) for s in ('entry','resume')]
    for name,stage in positive:assert parse(name+'\t'+stage+suffix)['stage']==stage
    for name,stage in [('Sweep','admitted'),('Other','resume'),('Factor','entry'),
        ('congruence-dense-resume','resume'),('vivify-dense-resume','first')]:
        try:parse(name+'\t'+stage+suffix)
        except AssertionError:pass
        else:raise AssertionError('Unrecognized producer/stage accepted')
    print('PASS:24 ownership fixtures;only source-backed extra producer/stage pairs',flush=True)
if __name__=='__main__':fixtures()
