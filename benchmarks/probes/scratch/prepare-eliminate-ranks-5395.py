#!/usr/bin/env python3
"""Preserve the fixed full-view protocol for the coupled dense-entry candidate."""
from pathlib import Path
source = Path(__file__).with_name('prepare-vivify-ranks-5383.py').read_text()
for before, after in [
    ('20260907-5379-vivify-retry-runtime', '20260907-5392-eliminate-entry-runtime'),
    ('20260907-5383-vivify-retry-ranks1-40', '20260907-5395-eliminate-entry-ranks1-40'),
    ('Vivify retry stack', 'physical dense-entry stack'),
    ('build/pin-5379-vivify-retry-release', 'build/pin-5392-eliminate-dense-release'),
]:
    assert source.count(before) == 1, before
    source = source.replace(before, after)
exec(compile(source, __file__, 'exec'))
