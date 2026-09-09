#!/usr/bin/env python3
"""Apply tested full-stream framing/overlap checks to6305 and cached native."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank32-extension-6187.py')
code=source.read_text()
changes={
    "c = root / 'benchmarks/runs/20260909-6183-rank32-current-prefix'":
    "c = root / 'benchmarks/runs/20260909-6321-collapse-rank32-prefix'\nnative = root / 'benchmarks/runs/20260909-6303-rank32-later-prefix'\nif not (c/'results/candidate.json').exists():\n    print('Candidate still pending'); sys.exit(0)",
    '20260908-6060-root-lock-rank32-prefix':'20260909-6276-vivify-binary-rank32-prefix',
    "old['complete_trace_sha256'][stream.name]":"old['hashes'][str(stream.relative_to(root))]",
    "old['matching_decisions']":"old['matching_decisions_from_start']",
    '69828932':'96083379', '^300000\\t':'^1000000\\t', "b'300000\\t'":"b'1000000\\t'",
    "old['new_prefix']":"old['prefixes']['candidate']",'stop=1000000':'stop=3000000',
    "'10d4b4ca139060f5'":"'33974c0cb2f02138'",
    "for arm in ('candidate', 'kissat'):\n    p = c / 'results'":
    "for arm in ('candidate', 'kissat'):\n    d = c if arm == 'candidate' else native\n    p = d / 'results'",
    "sha(c / 'pins' / arm)":"sha(d / 'pins' / arm)",
    "p = c / 'traces' / name":"p = d / 'traces' / name",
    "read_prefix(c / 'traces' / (arm + '.prefix.tsv'))":"read_prefix(d / 'traces' / (arm + '.prefix.tsv'))",
    "stream = c / 'traces' / (arm + '.decisions.tsv')":"stream = d / 'traces' / (arm + '.decisions.tsv')",
    "with (c / 'traces' / (arm + '.boundaries.tsv')).open() as inp:":"with (d / 'traces' / (arm + '.boundaries.tsv')).open() as inp:",
    "c / 'traces/kissat.decisions.tsv'":"native / 'traces/kissat.decisions.tsv'",
}
for old,new in changes.items():
    assert old in code,old
    code=code.replace(old,new)
# A semantic source change can invalidate the inherited-prefix premise.
# Do not misclassify that as solver or proof invalidity or publish localization.
marker="cached = root /"
assert code.count(marker)==1
check="""binding=m['cached_native']; assert sha(root/binding['path'])==binding['sha256']
current=read_prefix(c/'traces/candidate.prefix.tsv')
if current['common_reached']!=1 or current['common_hash']!='33974c0cb2f02138':
    print(json.dumps(dict(status='earlier_prefix_changed',prefix=current,
        next='Run an earlier current-source prefix; do not attribute the old1368117 split to6305.')))
    sys.exit(0)
"""
code=code.replace(marker,check+marker)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
