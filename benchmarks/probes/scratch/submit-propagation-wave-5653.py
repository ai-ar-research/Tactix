#!/usr/bin/env python3
"""Advance exact-work timing after solver outcomes, without waiting for checks."""
from pathlib import Path
source = Path(__file__).with_name('submit-factor-support-wave-5603.py').read_text()
source = source.replace('20260908-5599-factor-support-rank32-runtime', '20260908-5652-propagation-cursor-rank39-runtime')
source = source.replace('20260908-5598-factor-support-BC-gate', '20260908-5651-propagation-cursor-BC-gate')
anchor = "        bound[str(paths[i].relative_to(root))] = sha(paths[i])"
assert source.count(anchor) == 1
source = source.replace(anchor, anchor + "\n    assert stats(rows[0]) == stats(rows[1]) and rows[0]['certificate_sha256'] == rows[1]['certificate_sha256']")
exec(compile(source, __file__, 'exec'))
