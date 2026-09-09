#!/usr/bin/env python3
"""Successor preserves6342's rejected single-invocation accounting assumption."""
from pathlib import Path

source = Path(__file__).with_name('validate-rank32-transitive-6342.py')
code = source.read_text()
old = "assert sum(reduced['candidate'].values())==1 and sum(reduced['kissat'].values())==2,'Boundary/census reduction mismatch'"
assert code.count(old) == 1
new = """# Both current parent boundary logs contain TWO calls at1368343.
# First is121/121; second is1/2. Conflict-filtered census covers both.
import csv
from transitive_trace import binary_drops
invocations={}
for arm,name in (('candidate','TransitiveReduce'),('kissat','transitive')):
    with (c/'traces'/f'{arm}.boundaries.tsv').open() as stream:
        assert next(stream)==f'# {"tactix" if arm=="candidate" else "kissat"}-inprocess-boundary\\t1\\n'
        fields=next(stream).removeprefix('# ').rstrip('\\n').split('\\t')
        rows=list(csv.DictReader(stream,fieldnames=fields,delimiter='\\t'))
    invocations[arm]=binary_drops(rows,name,m['diagnostic_conflict'])
    assert [v['drop'] for v in invocations[arm]]==([121,1] if arm=='candidate' else [121,2])
    assert not any(r['failed'] for r in candidates[arm]),'Unit effects require separate accounting'
    assert sum(reduced[arm].values())==sum(v['drop'] for v in invocations[arm]),'Full conflict reduction mismatch'
"""
code = code.replace(old, new)
marker = 'result=dict(parent_replay=True,conflict=m[\'diagnostic_conflict\'],'
assert code.count(marker) == 1
code = code.replace(marker, marker + "\n    invocations=invocations, accounting_correction='6342 passed full per-arm trace/proof replay and census framing, then rejected122/123 total reductions against an incorrect1/2 assumption. Both invocations share1368343; first121/121 and second1/2. Successor6344 validates all begin/end frames and totals, with two new regression tests. No solver/proof failure; original6342 retained unchanged.',")
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
