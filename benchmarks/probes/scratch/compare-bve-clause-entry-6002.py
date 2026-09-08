#!/usr/bin/env python3
"""Require full replay and exact first-forward entry boundary coverage."""
from pathlib import Path
source = Path(__file__).with_name('compare-second-forward-formulas-5934.py')
code = source.read_text()
changes = {
    "for number,stage in ((5931,'begin'),(5932,'end')):": "for number,stage in ((6000,'begin'),):",
    "f'benchmarks/runs/20260908-{number}-second-forward-{stage}-formula'": "'benchmarks/runs/20260908-6000-bve-clause-entry'",
    "boundary=select(list(csv.DictReader(stream,fieldnames=header,delimiter='\\t')),stage)": "boundary=select(list(csv.DictReader(stream,fieldnames=header,delimiter='\\t')),stage,0)",
    "=={arm+'.formula.tsv'}": "=={arm+'.formula.tsv',arm+'.binaries.tsv',arm+'.bve-candidates.tsv'}",
}
for old, new in changes.items():
    assert old in code, old
    code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
