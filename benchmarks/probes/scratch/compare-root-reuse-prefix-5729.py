#!/usr/bin/env python3
"""Validate native parent replay before comparing the repaired decision suffix."""
from pathlib import Path
source=Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source=source.replace('20260907-5436-rank37-inprocess-prefix','20260908-5726-root-reuse-rank33-prefix')
source=source.replace('5436 pending','5726 pending')
anchor='count, first = 0, None'
assert source.count(anchor)==1
source=source.replace(anchor,"""parent=root/m['parent_campaign']
assert sha(parent/'summary.json')==m['parent_summary_sha256']
old=json.loads((parent/'results/kissat.json').read_text())
native=json.loads((c/'results/kissat.json').read_text())
assert old['partial_certificate']==native['partial_certificate']
assert old['trace_sha256']==native['trace_sha256']
for name,digest in old['trace_sha256'].items():assert sha(parent/'traces'/name)==digest
early_common=all(prefix[6]=='1' and prefix[5]==str(m['common_decisions']) for prefix in prefixes.values())
early_common=early_common and prefixes['candidate'][7:10]==prefixes['kissat'][7:10]
"""+anchor)
source=source.replace('result = dict(clean_stops=2,','result = dict(native_parent_replay=True,earlier_common_literal_digest_equal=early_common,stream_from=m[\'stream_from\'],clean_stops=2,')
source=source.replace('Stopped proof-on diagnostic only.','matching_rows counts the recorded suffix from100K only;earlier common literal digest is reported separately. Stopped proof-on diagnostic only.')
exec(compile(source,__file__,'exec'))
