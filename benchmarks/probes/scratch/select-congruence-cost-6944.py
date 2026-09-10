#!/usr/bin/env python3
"""Select a Rank41-50 unit-cost diagnostic without refreshing the denominator."""
from pathlib import Path
import json
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from runtime_summary import counters
from inprocessing_cost import read_timers
from checked_comparison import classify
c = root/'benchmarks/runs/20260910-6944-congruence-cost-selection'
assert not c.exists()
paths = [Path(__file__),root/'tools/bench/inprocessing_cost.py',root/'tools/bench/runtime_summary.py',root/'tools/bench/checked_comparison.py']
fixed_path = root/'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json'
fixed = json.loads(fixed_path.read_text()); paths.append(fixed_path)
assert fixed['solver_seconds'] == 7239.01
rows = {}
for rank in (42,43,47):
    current_path = root/f'benchmarks/runs/20260909-6743-reduce-watermark-ranks1-50/results/{rank-1:03d}.json'
    native_path = root/f'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50/results/{2*rank-1:03d}.json'
    paths.extend((current_path,native_path))
    current = json.loads(current_path.read_text()); native = json.loads(native_path.read_text())
    assert classify(current,True)['checked'] and classify(native,True)['checked']
    assert current['rank'] == native['rank'] == rank and current['instance'] == native['instance']
    assert current['expected'] == native['expected'] == 'UNSAT'
    ref = next(r for r in fixed['records'] if r['rank']==rank)
    rows[rank] = dict(current=dict(path=str(current_path.relative_to(root)),work=counters(current),
        solve=current['solve'],timers=read_timers(current['solver_statistics']),
        congruence_statistics=[s for s in current['solver_statistics'] if s.startswith('c congruence ')]),
        native=dict(path=str(native_path.relative_to(root)),work=counters(native),solve=native['solve'],
        congruence_statistics=[s for s in native['solver_statistics'] if 'congru' in s.lower() and not s.startswith('c --')]),
        fixed_native_seconds=ref['measurement']['solve']['wall_seconds'])
c.mkdir()
with (c/'summary.json').open('x') as stream:
    json.dump(dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},rows=rows,selected_ranks=[47],
        hypothesis='Rank47 current spends58.730 steady-clock seconds in Congruence, five calls; fixed native reports40.98 nestedCPU seconds andfive closures. Current35.37M versusnative38.19M reported gates and different merge/unit/search work prevent a precise unit-price claim. Profile same immutable current/native binaries to identify physical scanning/allocation work; require exact own unprofiled counters/certificate replay and zero lost samples.',
        nonselection='42/43 have four/five currentCongruence calls versus three fixed-native closures and roughly2x/3x conflicts, so their large runtime ratios do not isolate unit cost. Keep these scheduling/CDCL gaps open; do not narrow the Rank1-50 goal or resume shelvedRank21.',
        contract='One two-task Rank47 diagnostic with unchanged199Hz cycles:u/DWARF8192 wrapper,12CPU/48000MiB/1200solver/16000checker/5h,fullwidth/no hardnode. Diagnostic native run allowed; fixed7239.010 denominator immutable. No source change, promotion, fuzz access or FlatLists redesign.',
        test_consideration='No new parser behavior: reuse maintained timer/counter/checked-result readers and existing immutable sampling wrapper. Selection is exact pinned-record assertions, not a new acceptance threshold. Any later implementation change needs new unit-coverage consideration.'),stream,indent=2)
    stream.write('\n')
print(c)
