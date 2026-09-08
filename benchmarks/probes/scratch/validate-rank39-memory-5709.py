#!/usr/bin/env python3
"""Validate the pinned runner's distinct provisional and final record schemas."""
from pathlib import Path
import runpy


def identity(record, manifest, final):
    arm = manifest['arms']['candidate']
    assert record['arm'] == 'candidate' and record['task'] == 0
    assert record['rank'] == 39 and record['instance'] in manifest['input_sha256']
    assert manifest['pin_sha256'][arm['path']] == arm['sha256']
    if final:
        assert record['schema'] == 'tactix-matched-slurm-memory-regression-result-v2'
        assert record['binary_sha256'] == arm['sha256']
    else:
        assert record['schema'] == 'tactix-matched-slurm-provisional-result-v1'
        assert record['validation_pending'] is True
        assert 'binary_sha256' not in record


def schema_fixtures():
    import copy
    m = dict(arms={'candidate':dict(path='pins/candidate',sha256='abc')},
             pin_sha256={'pins/candidate':'abc'},input_sha256={'input':'def'})
    p = dict(arm='candidate',task=0,rank=39,instance='input',validation_pending=True,
             schema='tactix-matched-slurm-provisional-result-v1')
    f = dict(p,schema='tactix-matched-slurm-memory-regression-result-v2',binary_sha256='abc')
    identity(p,m,False);identity(f,m,True)
    cases = [(dict(p,arm='kissat'),m,False),(dict(p,task=1),m,False),
             (dict(p,rank=38),m,False),(dict(p,instance='other'),m,False),
             (dict(p,validation_pending=False),m,False),
             (dict(p,binary_sha256='abc'),m,False),(dict(p,schema=f['schema']),m,False),
             (dict(f,binary_sha256='wrong'),m,True),(p,m,True),(f,m,False)]
    wrong=copy.deepcopy(m);wrong['pin_sha256']['pins/candidate']='wrong'
    cases.append((p,wrong,False))
    for record,manifest,final in cases:
        try:identity(record,manifest,final)
        except (AssertionError,KeyError):pass
        else:raise AssertionError('Malformed record identity accepted')
    print('PASS:13 provisional/final identity fixtures',flush=True)


source=Path(__file__).with_name('validate-rank39-memory-5688.py').read_text()
anchor="    assert r['binary_sha256']==m['arms']['candidate']['sha256']"
assert source.count(anchor)==1
source=source.replace(anchor,"    identity(r,m,final.exists())")
anchor="if __name__=='__main__':main()"
assert source.count(anchor)==1
source=source.replace(anchor,"""support=runpy.run_path(str(Path(__file__).with_name('memory-row-parser-5706.py')))
parse=support['parse']
def fixtures():
    support['fixtures']()
    schema_fixtures()
if __name__=='__main__':main()""")
source=source.replace('parser_fixtures=11','parser_fixtures=24,record_schema_fixtures=13')
exec(compile(source,__file__,'exec'))
