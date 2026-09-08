#!/usr/bin/env python3
"""Use the existing strict B28+C5 result/checker/cleanup gate without changing tiers."""
from pathlib import Path
source = Path(__file__).with_name('seal-vivify-census-5553.py').read_text()
source = source.replace('20260908-5550-vivify-census-BC-gate', '20260908-5598-factor-support-BC-gate')
exec(compile(source, __file__, 'exec'))
