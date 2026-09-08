#!/usr/bin/env python3
"""Score all three behavior-preserving pairs with cross-wave identity too."""
from pathlib import Path
source = Path(__file__).with_name('snapshot-watch-runtime-5428.py').read_text()
source = source.replace('20260907-5428-watch-truncate-rank35-runtime', '20260908-5551-vivify-census-rank39-runtime')
source = source.replace('5428 solver results pending', '5551 solver results pending')
needle = "    assert t['certificate_sha256']==k['certificate_sha256'], 'Exact proof identity premise failed'"
assert needle in source
source = source.replace(needle, needle + "\n    if wave:\n        assert stats(t)==stats(rows[0])\n        assert t['certificate_sha256']==rows[0]['certificate_sha256']")
exec(compile(source, __file__, 'exec'))
