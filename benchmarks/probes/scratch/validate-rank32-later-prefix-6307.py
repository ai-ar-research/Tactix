#!/usr/bin/env python3
"""Require the full inherited1M closure and actual overlap before comparison."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank32-extension-6187.py')
code=source.read_text()
for a,b in (
    ('20260909-6183-rank32-current-prefix','20260909-6303-rank32-later-prefix'),
    ('20260908-6060-root-lock-rank32-prefix','20260909-6276-vivify-binary-rank32-prefix'),
    ("old['complete_trace_sha256'][stream.name]","old['hashes'][str(stream.relative_to(root))]"),
    ("old['matching_decisions']","old['matching_decisions_from_start']"),
    ('69828932','96083379'),('^300000\\t','^1000000\\t'),("b'300000\\t'","b'1000000\\t'"),
    ("old['new_prefix']","old['prefixes']['candidate']"),
    ('stop=1000000','stop=3000000'),("'10d4b4ca139060f5'","'33974c0cb2f02138'")):
    assert a in code,a;code=code.replace(a,b)
marker='difference = first_stream_difference('
assert code.count(marker)==1
extra="""# Different stops prevent proof-byte equality. Ordinary coarse streams
# nevertheless must preserve every cached row independently on each arm.
for arm in ('candidate','kissat'):
    parent=root/m['parent_runs'][arm]
    previous=json.loads((parent/'results'/f'{arm}.json').read_text())
    current=json.loads((c/'results'/f'{arm}.json').read_text())
    assert previous['binary_sha256']==current['binary_sha256']
    for name,digest in previous['trace_sha256'].items():
        if name.endswith(('.decisions.tsv','.prefix.tsv')):continue
        p=parent/'traces'/name;assert sha(p)==digest
        hashes[str(p.relative_to(root))]=digest
        with p.open('rb') as before,(c/'traces'/name).open('rb') as after:
            while block:=before.read(1<<20):assert after.read(len(block))==block,name
"""
code=code.replace(marker,extra+marker)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
