#!/usr/bin/env python3
"""Capture the propagation-order split before the selected clause pose changes."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-search-window-6676.py')
code=source.read_text()
changes={
    '20260909-6676-rank46-search-window':'20260909-6705-rank46-propagation-split',
    "'tactix-6676-'":"'tactix-6705-'","replace('6673','6676')":"replace('6673','6705')",
    '108330':'107100','108341':'107106',
    'Rank46 decisions largely agree through100K apart from two-row swap; later split108339/403138 picks13046 versus12721,focused. Inspect conflict-analysis/implication state before this split, without treating earlier swap or BVE99580 as the cause.':
        'Validated6704 all-scan clause history:260 reasonB observations match,then107104/393591/level1267 scans-12728 with identical pose/cursor but19005 false only in current. First14243 variable-filtered rows agree; current next scans-12750 where native next scans-12728. Capture107100..107106 implications/conflict/analysis to locate ordering or repropagation cause before proposing cursor/pose repair.',
    'Observer configuration only;196 benchmark tests, including six prefix-byte controls and seven search-window/replay fixtures. Add fixed regression if a source defect is isolated.':
        'Configuration-only reuse of existing search and exact-replay hooks;213 benchmark tests cover all maintained logic. No new unit test needed for window selection. A source repair requires its own regression.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
anchor="(c/'run.py').write_text(runner)"; assert code.count(anchor)==1
code=code.replace(anchor,"""p=root/'benchmarks/runs/20260909-6702-rank46-allscan-reason/summary-6704.json'
state=json.loads(p.read_text()); assert state['observer_replay_validated']
for rel,digest in state['hashes'].items(): assert sha(root/rel)==digest,rel
m['dependencies'][str(p.relative_to(root))]=sha(p)
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
