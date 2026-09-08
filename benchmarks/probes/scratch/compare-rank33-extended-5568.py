#!/usr/bin/env python3
"""Validate the recorded suffix, old common literal prefix, and prior boundaries."""
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5567-rank33-small-extended'
m = json.loads((c / 'manifest.json').read_text())
parent = root / m['parent_campaign']
assert hashlib.sha256((parent / 'summary.json').read_bytes()).hexdigest() == m['prior_summary_sha256']
prior = json.loads((parent / 'summary.json').read_text())
assert prior['complete_prefix_identical'] and prior['matching_rows'] == 31494584
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix', '20260908-5567-rank33-small-extended')
source = source.replace('5436 pending', '5567 pending')
anchor = "count, first = 0, None"
assert source.count(anchor) == 1
source = source.replace(anchor, '''
for arm,prefix in prefixes.items():
    old_prefix = prior['prefixes'][arm]
    assert int(prefix[5]) == int(old_prefix[3]) == m['common_decisions']
    assert prefix[6] == '1' and prefix[7] == old_prefix[4]
    assert prefix[8] == old_prefix[8] and prefix[9] == old_prefix[10]
    for suffix in ('boundaries.tsv', 'macro.tsv'):
        previous = parent / 'traces' / (arm + '.' + suffix)
        if not previous.exists():
            assert arm == 'kissat' and suffix == 'macro.tsv'
            continue
        old_lines = previous.read_bytes().splitlines()
        with (c / 'traces' / (arm + '.' + suffix)).open('rb') as current:
            for line in old_lines: assert next(current).rstrip(b'\\n') == line
''' + anchor)
source = source.replace('result = dict(clean_stops=2,',
    "result = dict(stream_from=m['stream_from'], prior_summary_sha256=m['prior_summary_sha256'], prior_common_prefix_replayed=True, clean_stops=2,")
source = source.replace('Stopped proof-on diagnostic only.',
    'matching_rows counts only the suffix from100K. Prior31494584-literal prefix digest and all previous boundary rows replay exactly. Stopped proof-on diagnostic only.')
exec(compile(source, __file__, 'exec'))
