#!/usr/bin/env python3
"""Bracket the first observed selected-scan pose change with full search detail."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-search-window-6676.py')
code=source.read_text()
changes={
    '20260909-6676-rank46-search-window':'20260909-6688-rank46-earlier-window',
    "'tactix-6676-'":"'tactix-6688-'",
    "replace('6673','6676')":"replace('6673','6688')",
    '108330':'108080','108341':'108170','250000':'500000',
    'Rank46 decisions largely agree through100K apart from two-row swap; later split108339/403138 picks13046 versus12721,focused. Inspect conflict-analysis/implication state before this split, without treating earlier swap or BVE99580 as the cause.':
        'Validated6684/6686 and6687 show177292 identical portable-16719 watch rows from startup,then first32-literal clause pose change108138. The eventual21-literal conflicting clause matches108086, differs108162. Capture full search108080..108170 to locate earlier implications/conflicts; do not assume this is the first global divergence.',
    'Observer configuration only;196 benchmark tests, including six prefix-byte controls and seven search-window/replay fixtures. Add fixed regression if a source defect is isolated.':
        'Observer configuration only;207 benchmark tests include strict history projection and duplicate preservation. Existing search-window replay fixtures apply. Add a fixed regression if a source defect is isolated.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
anchor="(c/'run.py').write_text(runner)"; assert code.count(anchor)==1
code=code.replace(anchor,"""p=root/'benchmarks/runs/20260909-6684-rank46-watch-history/localization-6687.json'
analysis=json.loads(p.read_text()); assert analysis['complete_portable_common_rows']==177292
for rel,digest in analysis['hashes'].items(): assert sha(root/rel)==digest,rel
m['dependencies'][str(p.relative_to(root))]=sha(p)
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
