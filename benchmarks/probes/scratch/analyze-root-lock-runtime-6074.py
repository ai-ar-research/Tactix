#!/usr/bin/env python3
"""Bind full40 work, memory and descriptive phase costs without repricing time."""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
import fixed_runtime
c = root / 'benchmarks/runs/20260908-6061-root-lock-ranks1-40'
parent = root / 'benchmarks/runs/20260908-5970-bve-active-pop-ranks1-40'
target = c / 'work-cost-summary.json'
assert not target.exists()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
score = json.loads((c / 'solver-checkpoint.json').read_text())
assert score['records'] == score['expected_solves'] == 40 and not score['runtime_target_met']
reference = fixed_runtime.reference()
hashes = {str((c / 'solver-checkpoint.json').relative_to(root)): sha(c / 'solver-checkpoint.json')}
verdicts = Counter()
rows = []
changed = []
for rank in range(1, 41):
    p, q = c / 'provisional' / f'{rank-1:03}.json', parent / 'provisional' / f'{rank-1:03}.json'
    a, b = json.loads(p.read_text()), json.loads(q.read_text())
    native = next(r for r in reference['records'] if r['rank'] == rank)['measurement']
    assert a['instance'] == b['instance'] == native['instance']
    assert a['expected_match'] and not a['solve']['timed_out'] and a['solve']['exit_code'] in (10, 20)
    verdicts[a['verdict']] += 1
    hashes[str(p.relative_to(root))] = sha(p)
    hashes[str(q.relative_to(root))] = sha(q)
    if a['certificate_sha256'] != b['certificate_sha256']:
        changed.append(rank)
    if rank not in (21, 27, 31, 32, 37):
        continue
    stats = {}
    for solver, record in (('candidate', a), ('kissat', native)):
        counts = {}
        for name in ('decisions', 'conflicts', 'propagations'):
            pattern = r'^c ' + name + (r':\s+(\d+)' if solver == 'kissat' else r'\s+(\d+)')
            matches = [re.match(pattern, line) for line in record['solver_statistics']]
            values = [int(m[1]) for m in matches if m]
            assert len(values) == 1, (rank, solver, name)
            counts[name] = values[0]
        stats[solver] = counts
    rows.append(dict(rank=rank, candidate_seconds=a['solve']['wall_seconds'], parent_seconds=b['solve']['wall_seconds'],
        kissat_seconds=native['solve']['wall_seconds'], counters=stats,
        work_ratios={k: stats['candidate'][k] / stats['kissat'][k] for k in stats['candidate']},
        candidate_rss_kb=a['solve']['peak_rss_kb'], kissat_rss_kb=native['solve']['peak_rss_kb'],
        candidate_timing_lines=[line for line in a['solver_statistics'] if line.startswith('c timing')],
        native_timing_lines=[line for line in native['solver_statistics'] if re.match(r'^c\s+\d+\.\d+\s+\d+\.\d+ %', line)]))
records = [json.loads(p.read_text()) for p in (c / 'provisional').glob('*.json')]
maximum = max(records, key=lambda r: r['solve']['peak_rss_kb'])
finals = list((c / 'results').glob('*.json'))
for p in finals:
    r = json.loads(p.read_text())
    assert r['valid'] and r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending']
out = dict(verdicts=dict(verdicts), expected_solves=40, checked_clean=len(finals),
    ratio=score['ratio'], candidate_seconds=score['candidate_par2'], fixed_kissat_seconds=score['fixed_kissat_seconds'],
    previous_seconds=sum(json.loads(p.read_text())['solve']['wall_seconds'] for p in (parent / 'provisional').glob('*.json')),
    changed_certificate_ranks=changed, max_rss_kb=maximum['solve']['peak_rss_kb'], max_rss_instance=maximum['instance'],
    selected_rows=rows, hashes=hashes,
    test_consideration='No solver change. Existing22 benchmark protocol tests cover fixed-reference scoring and trace framing. This frozen analysis asserts all40 expected verdicts, unique counter parsing, input identity and clean published certificates.',
    limitation='Cross-window fixed-reference aggregate,not isolated unit-cost attribution. Native and Tactix propagation counters and phase boundaries differ; timing lines are descriptive and overlapping native scopes must not be summed. Sub-16GiB measured RSS does not prove a hard-cgroup or comparative memory gate. Pending proofs and current-source fuzz remain unverified.')
target.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({k: v for k, v in out.items() if k not in ('hashes', 'selected_rows')}, indent=2))
