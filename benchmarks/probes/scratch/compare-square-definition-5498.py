#!/usr/bin/env python3
"""Validate native event-name boundaries without altering their raw metadata."""
from pathlib import Path
source = Path(__file__).with_name('compare-square-definition-5497.py').read_text()
source = source.replace('20260908-5497-square-definition-phase',
                        '20260908-5498-square-definition-selectors').replace('5497 pending', '5498 pending')
end = source.rindex("exec(compile(source, __file__, 'exec'))")
source = source[:end] + '''source = source.replace("assert int(row[4]) == 0 and 'stage=end' in row[6]",
    "assert int(row[4]) == 0 and ('stage=end' in row[6] or (solver == 'kissat' and row[3].endswith('_end') and row[6] == '-'))")
source = source.replace("    print('PASS: two positive/six negative selected snapshot fixtures')",
    """    native = '# begin\\\\t0\\\\tkissat\\\\tfactor_end\\\\t0\\\\t1\\\\t-\\\\n0\\\\t3270\\\\t43773\\\\n# end\\\\t0\\\\n'
    assert binary_snapshots(native, 'kissat')[0]['clauses'] == [[3270, 43773]]
    try:
        binary_snapshots(native.replace('factor_end', 'factor_begin'), 'kissat')
    except AssertionError:
        pass
    else:
        raise AssertionError('Native begin accepted as end')
    print('PASS: three positive/seven negative selected snapshot fixtures')""")
source = source.replace('snapshot_fixtures=8', 'snapshot_fixtures=10')
''' + source[end:]
exec(compile(source, __file__, 'exec'))
