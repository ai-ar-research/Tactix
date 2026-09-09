#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('prepare-substitute-retry-gates-6236.py')
code = source.read_text()
marker = "\nold = root/'benchmarks/runs/20260909-6183-rank32-current-prefix'"
assert code.count(marker) == 1
code = code.split(marker)[0]
for before, after in (
    ('20260909-6231-substitute-retry-build', '20260909-6357-compact-stamp-build'),
    ('20260909-6234-substitute-retry-references', '20260909-6361-compact-stamp-controls'),
    ('20260909-6236-substitute-retry-BC-gate', '20260909-6362-compact-stamp-BC-gate'),
    ('build/pin-6231-substitute-retry-', 'build/pin-6357-compact-stamp-'),
    ('canonical Substitute removal publication', 'retained compact queue stamp and detached-link preservation'),
):
    assert before in code
    code = code.replace(before, after)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
