#!/usr/bin/env python3
"""Localize portable boundaries on the two validated current-source prefixes."""
from pathlib import Path
base = Path(__file__).with_name('localize-bound-boundary-5463.py').read_text()
for campaign, count in [('20260908-5735-root-reuse-rank31-prefix', 76952511),
                        ('20260908-5726-root-reuse-rank33-prefix', 23612137)]:
    source = base.replace('20260907-5463-rank37-bound-prefix', campaign)
    source = source.replace("summary['matching_rows'] >= 7918162",
                            f"summary['matching_rows'] == {count}")
    exec(compile(source, __file__, 'exec'), {'__file__':__file__})
