#!/usr/bin/env python3
"""All fifty remain scored; derive priority views without changing membership."""
import json
import math
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
p = root / 'benchmarks/runs/20260909-6181-learned-sort-ranks1-50/solver-checkpoint.json'
source = json.loads(p.read_text()); score = source['score']
assert score['records'] == score['expected_solves'] == score['reference_checked'] == 50
assert source['all50_exact_parent_work_and_proof']
rows = score['per_rank']; assert [r['rank'] for r in rows] == list(range(1, 51))
def aggregate(rows):
    t = math.fsum(r['candidate_par2_seconds'] for r in rows)
    k = math.fsum(r['kissat_seconds'] for r in rows)
    return dict(ranks=[r['rank'] for r in rows], candidate_seconds=t, kissat_seconds=k,
                ratio=t/k, delta_seconds=t-k)
views = {name: aggregate(v) for name, v in (('all50', rows), ('original40', rows[:40]), ('added10', rows[40:]))}
assert math.isclose(views['all50']['candidate_seconds'], score['candidate_par2'], abs_tol=1e-9)
assert math.isclose(views['all50']['kissat_seconds'], score['fixed_kissat_seconds'], abs_tol=1e-9)
losses = [dict(r, delta_seconds=r['candidate_par2_seconds']-r['kissat_seconds'],
               ratio=r['candidate_par2_seconds']/r['kissat_seconds']) for r in rows]
losses.sort(key=lambda r: (-r['delta_seconds'], r['rank']))
result = dict(source=dict(path=str(p.relative_to(root)), sha256=sha(p)), views=views, descending_losses=losses,
    limitation='Complete expected-verdict solver-time snapshot; independent checking follows6197 and remains pending on latest source. Cross-window scores are not causal unit-cost evidence. All50 remain scored; these are priority views, not new benchmark sets.',
    decision='After the already-running Rank32 producer probe, investigate addedRank50 thenRank49: their absolute losses dominate the added10. Preserve memory and source-fuzz gates, no rank-based solver parameters.',
    test_consideration='No solver or protocol change. Assert exact50 membership, expected outcomes, checked fixed reference and aggregate recomposition;39 fixed-runtime/classifier tests remain the scoring coverage.')
target = root / 'benchmarks/reports/2026-09-09-rank50-loss-breakdown-6198.json'
assert not target.exists()
target.write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(views, indent=2))
