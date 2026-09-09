#!/usr/bin/env python3
"""Reuse complete threshold-overlap/stream/boundary validation for the later split."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank32-extension-6187.py')
code=source.read_text()
for old,new in {
 '20260909-6183-rank32-current-prefix':'20260909-6459-rank35-later-prefix',
 '20260908-6060-root-lock-rank32-prefix':'20260909-6389-rank35-stamp-extension',
 "old['complete_trace_sha256'][stream.name]":"old['hashes'][str(stream.relative_to(root))]",
 "old['matching_decisions']":"old['matching_decisions_from_start']",
 "old['new_prefix']":"old['prefixes']['candidate']",
 '69828932':'99197378',
 "'^300000\\t'":"'^1000000\\t'", "b'300000\\t'":"b'1000000\\t'",
 'stop=1000000':'stop=3000000',
 "'10d4b4ca139060f5'":"'140843d3c85c31da'",
 "'kissat'":"'previous'", "'traces/kissat.decisions.tsv'":"'traces/previous.decisions.tsv'",
 'rank=32':'rank=35',
}.items():
    assert old in code,old; code=code.replace(old,new)
code=code.replace("arm == 'previous'","False")
anchor="assert not (c / 'summary.json').exists()"; assert code.count(anchor)==1
code=code.replace(anchor,anchor+"\nif any(not (c/'results'/f'{a}.json').exists() for a in ('candidate','previous')):\n    print('Rank35 later prefix pending; no summary created'); sys.exit(0)")
anchor='cached = root /'; assert code.count(anchor)==1
check="""for arm in ('candidate','previous'):
    current=read_prefix(c/'traces'/f'{arm}.prefix.tsv')
    if current['common_reached']!=1 or current['common_hash']!='140843d3c85c31da':
        print(json.dumps(dict(status='earlier_prefix_changed',arm=arm,prefix=current,
            next='Localize current source earlier; do not inherit the old1M decision closure.')))
        sys.exit(0)
"""
code=code.replace(anchor,check+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
