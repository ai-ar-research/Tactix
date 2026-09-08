#!/usr/bin/env python3
"""Check the full recorded suffix and its already-validated prefix authority."""
import hashlib
import json
from pathlib import Path
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5478-rank37-bound-extended'
m = json.loads((c / 'manifest.json').read_text())
p = root / m['parent_campaign'] / 'summary.json'
assert hashlib.sha256(p.read_bytes()).hexdigest() == m['prior_summary_sha256']
prior = json.loads(p.read_text())
assert prior['complete_prefix_identical'] and prior['matching_rows'] == 35346388
source = Path(__file__).with_name('compare-rank37-prefix-5436.py').read_text()
source = source.replace('20260907-5436-rank37-inprocess-prefix',
                        '20260908-5478-rank37-bound-extended')
source = source.replace('5436 pending', '5478 pending')
source = source.replace('result = dict(clean_stops=2,',
    "result = dict(stream_from=m['stream_from'], prior_summary_sha256=m['prior_summary_sha256'], clean_stops=2,")
source = source.replace('Stopped proof-on diagnostic only.',
    'Recorded suffix begins at conflict 100000 after a separately validated common prefix; matching_rows counts suffix rows only. Stopped proof-on diagnostic only.')
exec(compile(source, __file__, 'exec'))
