#!/usr/bin/env python3
"""Permit only the source-backed lucky UNKNOWN payload at native end events."""
from pathlib import Path
source = Path(__file__).with_name('compare-square-definition-5498.py').read_text()
source = source.replace("and row[6] == '-'",
    "and (row[6] == '-' or (row[3] in ('lucky_early_end', 'lucky_late_end') and row[6] == 'unknown'))")
source = source.replace("    print('PASS: three positive/seven negative selected snapshot fixtures')",
    """    assert binary_snapshots(native.replace('factor_end', 'lucky_early_end').replace('1' + chr(9) + '-' + chr(10), '1' + chr(9) + 'unknown' + chr(10)), 'kissat')
    try:
        binary_snapshots(native.replace('1' + chr(9) + '-' + chr(10), '1' + chr(9) + 'unknown' + chr(10)), 'kissat')
    except AssertionError:
        pass
    else:
        raise AssertionError('Unknown payload accepted outside lucky boundary')
    print('PASS: four positive/eight negative selected snapshot fixtures')""")
source = source.replace('snapshot_fixtures=10', 'snapshot_fixtures=12')
exec(compile(source, __file__, 'exec'))
