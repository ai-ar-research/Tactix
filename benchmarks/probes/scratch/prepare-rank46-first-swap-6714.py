#!/usr/bin/env python3
"""Inspect the first decision swap without assuming it causes later drift."""
from pathlib import Path
source=Path(__file__).with_name('prepare-rank46-search-window-6676.py')
code=source.read_text()
changes={
    '20260909-6676-rank46-search-window':'20260909-6714-rank46-first-swap',
    "'tactix-6676-'":"'tactix-6714-'","replace('6673','6676')":"replace('6673','6714')",
    '108330':'78790','108341':'78798',
    'Rank46 decisions largely agree through100K apart from two-row swap; later split108339/403138 picks13046 versus12721,focused. Inspect conflict-analysis/implication state before this split, without treating earlier swap or BVE99580 as the cause.':
        'Inspect the earliest decision swap at78797/296583:current-18626 then-18594,native reverse. Later6710 proves reversed unit-clause watch order107104; both14-literal reasons contain18626 but that alone is not a causal link. Capture prior implications,learned clauses,analysis and branch state while6711 independently traces later clause history.',
    'Observer configuration only;196 benchmark tests, including six prefix-byte controls and seven search-window/replay fixtures. Add fixed regression if a source defect is isolated.':
        'Observer configuration only;213 benchmark tests pass, including exact original-row and certificate-prefix replay fixtures. No new logic requiring tests; any source repair needs a focused regression.'}
for before,after in changes.items():
    assert before in code,before
    code=code.replace(before,after)
anchor="(c/'run.py').write_text(runner)"; assert code.count(anchor)==1
code=code.replace(anchor,"""p=root/'benchmarks/runs/20260909-6708-rank46-implication-watch/summary-6710.json'
state=json.loads(p.read_text()); assert state['observer_replay_validated']
for rel,digest in state['hashes'].items(): assert sha(root/rel)==digest,rel
m['dependencies'][str(p.relative_to(root))]=sha(p)
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
