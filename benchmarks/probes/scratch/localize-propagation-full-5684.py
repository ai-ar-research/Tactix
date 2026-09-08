#!/usr/bin/env python3
"""Select losses and work/cost questions from the complete current40-pair view."""
from pathlib import Path
source = Path(__file__).with_name('localize-factor-full-5615.py').read_text()
source = source.replace('20260908-5604-factor-support-ranks1-40','20260908-5664-propagation-cursor-ranks1-40')
source = source.replace('20260908-5535-sweep-small-ranks1-40','20260908-5604-factor-support-ranks1-40')
source = source.replace('Complete5604 checkpoint pending','Complete5664 checkpoint pending')
exec(compile(source,__file__,'exec'))
