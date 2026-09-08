#!/usr/bin/env python3
"""Score all three pairs only under complete cross-arm/replica identity."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-watch-runtime-5428.py').read_text()
source = source.replace('20260907-5428-watch-truncate-rank35-runtime', '20260908-5652-propagation-cursor-rank39-runtime')
source = source.replace('5428 solver results pending', '5652 solver results pending')
anchor = "    ts,ks=t['solve']['wall_seconds'],k['solve']['wall_seconds']"
assert source.count(anchor) == 1
source = source.replace(anchor, """    if wave:
        assert stats(t) == stats(rows[0]) and stats(k) == stats(rows[1])
        assert t['certificate_sha256'] == rows[0]['certificate_sha256']
        assert k['certificate_sha256'] == rows[1]['certificate_sha256']
""" + anchor)
exec(compile(source, __file__, 'exec'))
