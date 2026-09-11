#!/usr/bin/env python3
"""Full paired optimized/trace suites and unsuppressed focused sanitizers."""
from pathlib import Path
base=Path(__file__).with_name('build-debug-contracts-7340.py')
code=base.read_text()
changes={
    'runtime-debug-contracts-7339':'runtime-lucky-roots-7365',
    '7340-debug-contracts':'7366-lucky-roots',
    "fixtures = ('root_fold', 'root_watermark'":"fixtures = ('lucky_roots', 'lucky', 'root_fold', 'root_watermark'",
    "count = 122 if kind == 'trace' else 123":"count = 123 if kind == 'trace' else 124",
    'fourteen fixed':'sixteen fixed',
}
for before,after in changes.items():
    assert before in code,before;code=code.replace(before,after)
exec(compile(code,str(base),'exec'),dict(__file__=str(Path(__file__)),__name__='__main__'))
