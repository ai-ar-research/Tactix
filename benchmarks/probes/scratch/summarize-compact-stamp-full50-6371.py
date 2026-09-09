#!/usr/bin/env python3
"""Complete fixed50 work/runtime/memory view, with fail-closed validation."""
import csv
import datetime as dt
import json
import math
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha, score, reference
from checked_comparison import classify
from runtime_summary import counters
c = root/'benchmarks/runs/20260909-6366-compact-stamp-ranks1-50'
parent = root/'benchmarks/runs/20260909-6320-substitute-collapse-ranks1-50'
out = root/'benchmarks/reports/2026-09-09-compact-stamp-full50-6371'
assert not out.exists()
s = score(c)
if not s['complete']:
    print('Full50 solver view pending:', s['records'], '/50; no output created')
    sys.exit(0)
assert s['expected_solves'] == 50
ref = reference()
hashes = {}
def bind(p):
    hashes[str(p.relative_to(root))] = sha(p)
def read(campaign, rank):
    p = campaign/'results'/f'{rank-1:03}.json'
    if not p.exists():
        p = campaign/'provisional'/p.name
    r = json.loads(p.read_text())
    state = classify(r, p.parent.name == 'results')
    assert state['status'] in ('pending_validation', 'checked_SAT', 'checked_UNSAT'), (p, state)
    bind(p)
    return r, state
for campaign in (c, parent):
    m = json.loads((campaign/'manifest.json').read_text())
    bind(campaign/'manifest.json'); bind(campaign/'launch.json')
    for rel, digest in m['pin_sha256'].items():
        assert sha(campaign/rel) == digest
bind(root/'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json')
rows = []
ratio = lambda a,b: a/b if b else None
for rank in range(1, 51):
    current, state = read(c, rank)
    previous, _ = read(parent, rank)
    native = ref['records'][rank-1]['measurement']
    assert ref['records'][rank-1]['rank'] == rank
    assert current['instance'] == previous['instance'] == native['instance']
    assert current['expected'] == previous['expected'] == native['expected']
    metrics = {a:counters(r) for a,r in (('candidate',current),('parent',previous),('kissat',native))}
    assert all(v is not None for values in metrics.values() for v in values.values()), rank
    times = {a:r['solve']['wall_seconds'] for a,r in (('candidate',current),('parent',previous),('kissat',native))}
    rss = {a:r['solve']['peak_rss_kb'] for a,r in (('candidate',current),('parent',previous),('kissat',native))}
    rows.append(dict(rank=rank, instance=current['instance'], expected=current['expected'],
        validation=state['status'], candidate_seconds=times['candidate'], parent_seconds=times['parent'],
        kissat_seconds=times['kissat'], candidate_over_kissat=ratio(times['candidate'], times['kissat']),
        candidate_over_parent=ratio(times['candidate'], times['parent']),
        gap_seconds=times['candidate']-times['kissat'], delta_parent_seconds=times['candidate']-times['parent'],
        **{a+'_'+k:v for a,values in metrics.items() for k,v in values.items()},
        **{a+'_peak_rss_kib':v for a,v in rss.items()},
        rss_over_kissat=ratio(rss['candidate'],rss['kissat']), rss_over_parent=ratio(rss['candidate'],rss['parent']),
        certificate_exact_parent=current['certificate_sha256']==previous['certificate_sha256'],
        non_timing_stats_exact_parent=[x for x in current['solver_statistics'] if not x.startswith('c timing')]==
            [x for x in previous['solver_statistics'] if not x.startswith('c timing')],
        candidate_node=current['node'], parent_node=previous['node'], kissat_node=native['node']))
groups = {}
for name, selected in (('all50',rows),('original40',rows[:40]),('added10',rows[40:])):
    totals = {a:math.fsum(r[a+'_seconds'] for r in selected) for a in ('candidate','parent','kissat')}
    groups[name] = dict(**totals, ratio=totals['candidate']/totals['kissat'],
        gap_seconds=totals['candidate']-totals['kissat'],
        delta_parent_seconds=totals['candidate']-totals['parent'])
assert math.isclose(groups['all50']['candidate'], s['candidate_par2'], abs_tol=1e-8)
result = dict(observed_at=dt.datetime.now(dt.timezone.utc).isoformat(), score=s, groups=groups,
    rows=rows, hashes=hashes,
    changed_certificate_ranks=[r['rank'] for r in rows if not r['certificate_exact_parent']],
    changed_non_timing_stat_ranks=[r['rank'] for r in rows if not r['non_timing_stats_exact_parent']],
    test_consideration='Reuse maintained fixed-runtime scoring and checked-result classification tests. Require all50 input identities, all reported counters and exact score/table totals. No new solver/parser/acceptance behavior.',
    limitation='Cross-window fixed-reference/full-parent view, not causal instruction cost. Reported propagation currencies and changed trajectories can differ. Pending checks are unverified; memory, repeated timing and latest-source fuzz remain independent gates.')
out.mkdir()
(out/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
with (out/'comparison.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
print(json.dumps({k:result[k] for k in ('groups','changed_certificate_ranks','changed_non_timing_stat_ranks')}, indent=2))
print('Checked', s['checked_clean'], '/50; reference checked', s['reference_checked'], '/50')
