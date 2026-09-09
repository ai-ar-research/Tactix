#!/usr/bin/env python3
"""Use the full overlap/frame validator with an immutable cached native arm."""
from pathlib import Path
source=Path(__file__).with_name('validate-rank32-extension-6187.py')
code=source.read_text()
changes={
    "c = root / 'benchmarks/runs/20260909-6183-rank32-current-prefix'":
    "c = root / 'benchmarks/runs/20260909-6237-substitute-retry-rank32-prefix'\nnative = root / 'benchmarks/runs/20260909-6183-rank32-current-prefix'\nif not (c / 'results/candidate.json').exists():\n    print('Candidate still pending'); sys.exit(0)",
    "for arm in ('candidate', 'kissat'):\n    p = c / 'results'":
    "for arm in ('candidate', 'kissat'):\n    d = c if arm == 'candidate' else native\n    p = d / 'results'",
    "sha(c / 'pins' / arm)":"sha(d / 'pins' / arm)",
    "p = c / 'traces' / name":"p = d / 'traces' / name",
    "read_prefix(c / 'traces' / (arm + '.prefix.tsv'))":"read_prefix(d / 'traces' / (arm + '.prefix.tsv'))",
    "stream = c / 'traces' / (arm + '.decisions.tsv')":"stream = d / 'traces' / (arm + '.decisions.tsv')",
    "with (c / 'traces' / (arm + '.boundaries.tsv')).open() as inp:":"with (d / 'traces' / (arm + '.boundaries.tsv')).open() as inp:",
    "c / 'traces/kissat.decisions.tsv'":"native / 'traces/kissat.decisions.tsv'",
}
for old,new in changes.items():
    assert code.count(old)==1,old
    code=code.replace(old,new)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
