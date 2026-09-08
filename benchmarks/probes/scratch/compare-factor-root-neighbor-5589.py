#!/usr/bin/env python3
"""Replay and compare the physical inactive-endpoint witness with the tested parser."""
from pathlib import Path
source = Path(__file__).with_name('compare-factor-literal-5586.py').read_text()
source = source.replace('20260908-5585-rank32-factor-literal', '20260908-5588-rank32-factor-root-neighbor')
source = source.replace('5585 results pending', '5588 results pending')
source = source.replace("    print('PASS: four positive/twelve negative occurrence and projection fixtures', flush=True)", """    signed = good.replace('41096', '-174483')
    assert len(groups(signed, -174483)[0]['entries']) == 2
    try: groups(signed, 41096)
    except AssertionError: pass
    else: raise AssertionError('Wrong signed selection accepted')
    print('PASS: five positive/thirteen negative occurrence and projection fixtures', flush=True)""")
source = source.replace('parser_fixtures=16', 'parser_fixtures=18')
exec(compile(source, __file__, 'exec'))
