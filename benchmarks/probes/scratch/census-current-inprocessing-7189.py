#!/usr/bin/env python3
"""Freeze the current full50 printed-cost census without claiming removable cost."""
from collections import Counter
import datetime as dt
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify

def costs(lines):
    result = {}
    for line in lines:
        match = re.fullmatch(r'c timing +([A-Za-z]+) +(\d+) ms \([^)]*\) in (\d+) applications', line)
        if match:
            name, ms, count = match.groups()
            assert name not in result, 'duplicate rule timer'
            result[name] = dict(ms=int(ms), applications=int(count))
    return result

def fixtures():
    row = 'c timing   Factor 123 ms ( 4.0%) in 8 applications'
    assert costs([row, 'c timing 456 ms solve']) == {'Factor': dict(ms=123, applications=8)}
    assert costs([]) == {}
    try: costs([row, row])
    except AssertionError: pass
    else: raise AssertionError('duplicate accepted')
    print('Printed cost census: rule/solve separation, absent timers and duplicate rejection PASS')

def main():
    assert sys.argv[1:] in ([], ['--self-test'])
    fixtures()
    if sys.argv[1:]: return
    target = root / 'benchmarks/reports/2026-09-10-current-inprocessing-census-7189.json'
    assert not target.exists()
    c = root / 'benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50'
    hashes, rows, totals = {}, {}, Counter()
    for rank in range(1, 51):
        p = c / f'results/{rank-1:03d}.json'; final = p.exists()
        if not final: p = c / f'provisional/{rank-1:03d}.json'
        r = json.loads(p.read_text()); state = classify(r, final)
        assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT')
        assert r['rank'] == rank and r['task'] == rank-1 and r['verdict'] == r['expected']
        data = costs(r['solver_statistics'])
        totals.update({name: value['ms'] for name, value in data.items()})
        rows[str(rank)] = dict(instance=r['instance'], wall_seconds=r['solve']['wall_seconds'],
            rules=data, classification=state, timing_rows=[s for s in r['solver_statistics'] if s.startswith('c timing')])
        hashes[str(p.relative_to(root))] = sha(p)
    wall = sum(r['wall_seconds'] for r in rows.values())
    assert abs(wall - 8072.059) < 1e-6
    hashes[str(Path(__file__).relative_to(root))] = sha(Path(__file__))
    data = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), hashes=hashes, rows=rows,
        all50_wall_seconds=wall, total_reported_rule_ms=dict(totals.most_common()),
        factor_reported_seconds=totals['Factor']/1000,
        factor_reported_fraction=totals['Factor']/1000/wall,
        interpretation='Printed individual-rule wall timers, not complete lifecycle profiles or causal removable cost. Missing timer rows remain absent, not asserted zero. All50 retained, including shelved21; no scoring exclusions. Rank35 exact-work control can test engineering cost, while Rank31 still has changed native search work. Even all215.195 reported Factor seconds are far less than760.6589 seconds still needed for the aggregate target.',
        next='Finish the already bounded arena experiment, report whole-runtime/RSS and complete Factor effect. Do not mistake it for the main convergence solution. Prioritize larger Vivify/Sweep/Congruence lifecycle costs and demonstrated Rank31/43 search-work gaps after this measurement; preserve prior rejected experiments and closures.')
    with target.open('x') as out: json.dump(data, out, indent=2)
    print(target); print('Reported rule ms:', dict(totals.most_common()))

if __name__ == '__main__': main()
