#!/usr/bin/env python3
"""Read every runtime/RSS repetition; optional once-only decision snapshot."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from inprocessing_cost import paired_costs
path=Path(__file__).with_name('assess-factor-arena-7192.py')
spec=importlib.util.spec_from_file_location('distribution_controls',path)
h=importlib.util.module_from_spec(spec); spec.loader.exec_module(h)

def main():
    assert sys.argv[1:] in ([],['--self-test'],['--seal'])
    h.fixtures()
    if sys.argv[1:]==['--self-test']: return
    c=root/'benchmarks/runs/20260911-7273-dense-realloc-runtime'
    target=root/'benchmarks/reports/2026-09-11-dense-realloc-assessment-7281.json'
    if '--seal' in sys.argv: assert not target.exists(),'successful assessment frozen'
    reader=Path(__file__).with_name('check-submit-dense-realloc-runtime-7273.py')
    subprocess.run([sys.executable,str(reader)],check=True)
    hashes={}
    def bind(p,expected=None):
        value=sha(p); assert expected is None or value==expected,p
        hashes[str(p.relative_to(root))]=value
    bind(c/'manifest.json'); m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['pin_sha256'].items(): bind(c/rel,digest)
    for rel,digest in m['dependencies'].items(): bind(root/rel,digest)
    rows,states=[],[]
    for task in range(30):
        p=c/f'results/{task:03d}.json'; final=p.exists()
        if not final: p=c/f'provisional/{task:03d}.json'
        if not p.exists():
            print('All30 solver outcomes not yet available; no assessment written'); return
        r=json.loads(p.read_text()); bind(p); state=classify(r,final)
        assert state['status'] in ('pending_validation','checked_SAT','checked_UNSAT')
        rows.append(r); states.append(state)
    ranks=[9,25,31,35,38]
    costs=paired_costs(rows,ranks,3,['BVE'])
    spreads={}
    for rank in ranks:
        arms={arm:[r['solve']['wall_seconds'] for r in rows if r['rank']==rank and r['arm']==arm]
              for arm in ('candidate','previous')}
        for values in arms.values(): h.distribution(values)
        values={arm:max(times)/min(times)-1 for arm,times in arms.items()}
        spreads[rank]=dict(by_arm=values,absolute_headline_stable=max(values.values())<=.02)
    for wave in range(3): bind(c/f'wave{wave}-submission.json')
    for p in (Path(__file__),path,reader,root/'tools/bench/inprocessing_cost.py',root/'tools/bench/test_inprocessing_cost.py'):
        bind(p)
    result=dict(hashes=hashes,runtime=costs,spreads=spreads,expected_outcomes=30,
        checked_clean=sum(s['checked'] for s in states),
        interpretation='Every predeclared pair retained. Prioritize9/25 peak-RSS benefit, but explicitly retain31 memory and38 outside-slot regressions. Raw counters/certificates exact, including physical dense moves. Paired median/full range and absolute-headline2% spread gate unchanged. No overall win inferred from memory-only improvements.',
        next='Combine the complete repeated tradeoff with7277 current peak ownership and7278/7279/7280 Rank38 code-cost evidence. Decide whether a complete full50 memory/runtime transfer or a principled repair is justified. No alignment tuning or favorable-repeat selection.',
        limitation='No full50 competitiveness, unrestricted correctness/fuzz, final proof completion, memory parity or promotion from this targeted assessment. Independent checkers proceed asynchronously.')
    for rank,data in costs.items(): print(rank,data['paired'],spreads[rank])
    if '--seal' in sys.argv:
        with target.open('x') as out: json.dump(result,out,indent=2)
        print(target)

if __name__=='__main__': main()
