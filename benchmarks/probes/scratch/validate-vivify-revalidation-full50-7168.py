#!/usr/bin/env python3
"""Read every available full-fifty result before scoring or freezing a table."""
import csv
import datetime
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from checked_comparison import classify
from fixed_runtime import sha, score, reference
reader = Path(__file__).with_name('submit-vivify-compact-wave-7150.py')
spec = importlib.util.spec_from_file_location('vivify_fingerprint', reader)
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
h.fixtures()
assert sys.argv[1:] in ([], ['--seal'], ['--self-test'])
if sys.argv[1:] == ['--self-test']:
    sys.exit(0)
c = root / 'benchmarks/runs/20260910-7167-vivify-revalidation-ranks1-50'
parent = root / 'benchmarks/runs/20260910-7053-collect-retry-ranks1-50'
target = root / 'benchmarks/reports/2026-09-10-vivify-revalidation-full50-7168'
if '--seal' in sys.argv:
    assert not target.exists()
hashes = {}
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
m = json.loads((c / 'manifest.json').read_text())
for p in (c / 'manifest.json', c / 'launch.json', parent / 'manifest.json', reader, Path(__file__)):
    bind(p)
for rel, digest in m['pin_sha256'].items(): bind(c / rel, digest)
ref = reference()
bind(root / m['fixed_reference']['path'], m['fixed_reference']['sha256'])
records, states, paths = {}, {}, {}
for folder in ('provisional', 'results'):
    for p in sorted((c / folder).glob('*.json')):
        r = json.loads(p.read_text())
        task = r['task']
        assert task == int(p.stem) == r['rank'] - 1 and 0 <= task < 50
        expected = ref['records'][task]
        assert expected['rank'] == r['rank']
        assert r['arm'] == 'tactix' and r['instance'] == expected['instance'] and r['expected'] == expected['expected']
        state = classify(r, folder == 'results')
        if task in records:
            assert h.fingerprint(r) == h.fingerprint(records[task]) and r['solve'] == records[task]['solve']
        if folder == 'results':
            assert r['binary_sha256'] == m['arms']['tactix']['sha256']
        bind(p)
        records[task], states[task], paths[task] = r, state, p
failures = {task: state for task, state in states.items()
            if state['status'] not in ('pending_validation', 'checked_SAT', 'checked_UNSAT')}
print('Published', len(records), '/50; checked-clean', sum(s['checked'] for s in states.values()), '/50')
assert not failures, ('Classify these losses/invalidities before scoring or dependent integration', failures)
rows, mismatches = [], []
for task, r in sorted(records.items()):
    p = parent / f'results/{task:03d}.json'
    old = json.loads(p.read_text())
    assert classify(old, True)['checked'] and classify(old, True)['clean']
    bind(p)
    exact = h.fingerprint(r) == h.fingerprint(old)
    if not exact: mismatches.append(r['rank'])
    native = ref['records'][task]['measurement']
    seconds, prior_seconds, native_seconds = (x['solve']['wall_seconds'] for x in (r, old, native))
    rows.append(dict(rank=r['rank'], instance=r['instance'], expected=r['expected'],
        validation=states[task]['status'], exact_parent_work_and_proof=exact,
        candidate_seconds=seconds, parent_seconds=prior_seconds, kissat_seconds=native_seconds,
        candidate_over_parent=seconds/prior_seconds, candidate_over_kissat=seconds/native_seconds,
        candidate_peak_rss_kib=r['solve']['peak_rss_kb'], parent_peak_rss_kib=old['solve']['peak_rss_kb'],
        kissat_peak_rss_kib=native['solve']['peak_rss_kb']))
print('Exact parent work/proofs', len(rows)-len(mismatches), '/', len(rows))
assert not mismatches, ('Investigate changed work/proofs before interpreting cost-only transfer', mismatches)
if len(records) < 50:
    print('Solver phase incomplete; no score/table written, proof checking does not block progress')
    sys.exit(0)
result = score(c)
assert result['complete'] and result['expected_solves'] == 50
print('Full50 candidate seconds', result['candidate_par2'], 'fixed-Kissat ratio', result['ratio'])
if '--seal' in sys.argv:
    target.mkdir()
    with (target / 'summary.json').open('x') as stream:
        json.dump(dict(observed_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            hashes=hashes, score=result, rows=rows, exact_parent_work_and_proofs=True,
            limitation='One fresh full-set transfer view against fixed native and checked7030; no causal speedup attribution from cross-window timing. Pending checks permit progress, not completed correctness/fuzz/memory/consolidation gates.'), stream, indent=2)
    with (target / 'comparison.tsv').open('x', newline='') as stream:
        w = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
        w.writeheader()
        w.writerows(rows)
    print(target)
