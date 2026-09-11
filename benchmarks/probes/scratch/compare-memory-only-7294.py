#!/usr/bin/env python3
"""Compare completed memory-only traces against exact Release peak witnesses."""
import importlib.util
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
path = Path(__file__).with_name('compare-realloc-memory-7282.py')
spec = importlib.util.spec_from_file_location('structural_memory', path)
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)


def reproduction(observed, release):
    assert set(observed) == set(release) == {'candidate', 'previous'}
    for values in (observed, release):
        assert all(isinstance(v, int) and not isinstance(v, bool) and v > 0
                   for v in values.values())
    delta = observed['candidate'] - observed['previous']
    reference_delta = release['candidate'] - release['previous']
    assert reference_delta > 0, 'this gate targets an actual adverse Release pair'
    return dict(observed_delta_kib=delta, release_delta_kib=reference_delta,
        adverse_direction_reproduced=delta > 0,
        absolute_peaks_exact=observed == release,
        observer_peak_errors_kib={arm: observed[arm] - release[arm] for arm in release},
        delta_error_kib=delta - reference_delta)


def fixtures():
    reference = dict(candidate=120, previous=100)
    assert reproduction(reference, reference)['absolute_peaks_exact']
    assert not reproduction(dict(candidate=95, previous=105), reference)['adverse_direction_reproduced']
    same_direction = reproduction(dict(candidate=110, previous=105), reference)
    assert same_direction['adverse_direction_reproduced']
    assert not same_direction['absolute_peaks_exact']
    assert same_direction['delta_error_kib'] == -15
    for observed, release in (({}, reference), (dict(candidate=0, previous=1), reference),
        (dict(candidate=True, previous=1), reference),
        (dict(candidate=1.0, previous=1), reference),
        (reference, dict(candidate=100, previous=120))):
        try:
            reproduction(observed, release)
        except AssertionError:
            pass
        else:
            raise AssertionError('invalid peak gate accepted')
    h.fixtures()
    print('Release reproduction: exact/reversed/direction-only and five invalid controls PASS')


def main():
    assert sys.argv[1:] in ([], ['--self-test'], ['--seal'])
    fixtures()
    if sys.argv[1:] == ['--self-test']:
        return
    c = root / 'benchmarks/runs/20260911-7286-memory-only-rank31'
    target = c / 'comparison-7294.json'
    if '--seal' in sys.argv:
        assert not target.exists(), 'successful comparison frozen'
    summary_path = c / 'summary.json'
    if not summary_path.exists():
        print('Await the complete exact-work memory summary; no partial log attribution')
        return
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p)
        assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    bind(summary_path)
    summary = json.loads(summary_path.read_text())
    for rel, digest in summary['hashes'].items():
        bind(root / rel, digest)
    manifest = json.loads((c / 'manifest.json').read_text())
    bind(c / 'manifest.json')
    rows, release = {}, {}
    for task, arm in enumerate(('candidate', 'previous')):
        p = c / f'logs/{task:03d}.slot-memory.tsv'
        bind(p, summary['hashes'][str(p.relative_to(root))])
        rows[arm] = h.h.collect.base.parse_trace(p.read_text())
        original = manifest['release_records'][arm]
        p = root / original['path']
        bind(p, original['sha256'])
        release[arm] = json.loads(p.read_text())['solve']['peak_rss_kb']
    count = h.compare(rows['candidate'], rows['previous'])
    observed = {r['arm']: r['solver_peak_rss_kib'] for r in summary['records']}
    gate = reproduction(observed, release)
    contexts = {}
    for arm in rows:
        at = max(range(count), key=lambda i: rows[arm][i]['max_rss_kib'])
        contexts[arm] = dict(first_peak_index=at,
            previous_pair={a: rows[a][at-1] if at else None for a in rows},
            peak_pair={a: rows[a][at] for a in rows})
    largest_current = max(range(count), key=lambda i:
        rows['candidate'][i]['current_rss_kib'] - rows['previous'][i]['current_rss_kib'])
    for p in (Path(__file__), path):
        bind(p)
    result = dict(hashes=hashes, exact_structural_rows=count, release_peaks_kib=release,
        observer_peaks_kib=observed, reproduction=gate, peak_contexts=contexts,
        largest_current_rss_gap_pair={arm: rows[arm][largest_current] for arm in rows},
        limitation='Matching structural rows is diagnostic evidence, not unobserved Release allocator-address identity. Adverse direction is necessary but not sufficient for exact peak attribution; absolute peak errors stay explicit. A high-water rise may precede its sampled row. No allocator mechanism, runtime gain, promotion or completed pending-proof acceptance is inferred.')
    print(json.dumps({k: result[k] for k in ('exact_structural_rows', 'release_peaks_kib',
        'observer_peaks_kib', 'reproduction', 'peak_contexts')}, indent=2))
    if '--seal' in sys.argv:
        with target.open('x') as out:
            json.dump(result, out, indent=2)
        print(target)


if __name__ == '__main__':
    main()
