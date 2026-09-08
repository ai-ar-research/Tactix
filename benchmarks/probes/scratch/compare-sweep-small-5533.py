#!/usr/bin/env python3
"""Reuse strict native replay and unconstrained candidate localization."""
from pathlib import Path
source = Path(__file__).with_name('compare-sweep-retained-5518.py').read_text()
source = source.replace('20260908-5515-sweep-retained-rank32-trace',
                        '20260908-5529-sweep-small-rank32-trace')
source = source.replace('entry eligibility closes all scheduling parity',
                        'small-environment solving closes all scheduling parity')
exec(compile(source, __file__, 'exec'))
