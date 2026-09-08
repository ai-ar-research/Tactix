#!/usr/bin/env python3
"""Inspect the complete 5672 view and verify its claimed work neutrality."""
from pathlib import Path
source = Path(__file__).with_name('localize-factor-full-5615.py').read_text()
source = source.replace('20260908-5604-factor-support-ranks1-40',
                        '20260908-5712-assignment-level-ranks1-40')
source = source.replace('20260908-5535-sweep-small-ranks1-40',
                        '20260908-5664-propagation-cursor-ranks1-40')
source = source.replace('Complete5604 checkpoint pending', 'Complete5712 checkpoint pending')
anchor = "losses.sort(key=lambda r:(-r['extra_seconds'],r['rank']))"
assert source.count(anchor) == 1
source = source.replace(anchor, """assert all(r['same_parent_certificate'] and not r['parent_statistics_differences']
           for r in losses), 'Assignment-level bound must preserve all40 parent work/proof records'
""" + anchor)
exec(compile(source, __file__, 'exec'))
