#!/usr/bin/env python3
"""Partition each matched Reduce/sparse/GC envelope with exact conservation."""
import importlib.util
import json
import lzma
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import Frame, Sample, samples
reader = Path(__file__).with_name('localize-factor-envelope-7171.py')
spec = importlib.util.spec_from_file_location('partition', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)


def scoped(stream, binary, ancestors):
    for sample in stream:
        want = iter(ancestors)
        expected = next(want, None)
        for frame in reversed(sample.frames):
            if expected and frame.dso == binary and frame.symbol.startswith(expected):
                expected = next(want, None)
        if expected is None:
            yield sample


def self_test():
    h.self_test()
    f = lambda s, d='solver': Frame(1, s, d)
    data = [Sample(2, (f('gc'), f('collect'), f('reduce'))),
            Sample(3, (f('gc'), f('collect'))),
            Sample(5, (f('gc'), f('reduce'), f('collect'))),
            Sample(7, (f('gc'), f('collect', 'other'), f('reduce'))),
            Sample(11, (f('malloc', 'libc'), f('gc'), f('collect'), f('reduce')))]
    kept = list(scoped(data, 'solver', ('reduce', 'collect')))
    assert kept == [data[0], data[4]]
    value = h.summarize(kept, 'solver', lambda s: s == 'gc', {}, None, None)
    assert value['selected_period'] == value['total_period'] == 13
    assert value['branches']['no_phase_frame'] == {'<entry self>': 2, 'malloc [libc]': 11}
    assert list(scoped(data, 'solver', ())) == data
    print('Nested scope: missing/reversed/foreign ancestry excluded; descendants conserved PASS')


def main():
    self_test()
    assert len(sys.argv) == 2 and sys.argv[1] in ('--self-test', '0', '1', '2')
    if sys.argv[1] == '--self-test': return
    wave = int(sys.argv[1])
    target = root / f'benchmarks/reports/2026-09-11-root-binary-collection-7236-wave{wave}.json'
    assert not target.exists(), 'successful writer frozen'
    c = root / 'benchmarks/runs/20260911-7230-root-binary-profiles'
    extracts = root / f'benchmarks/runs/20260911-7231-root-binary-samples/wave{wave}'
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    def read(p):
        bind(p); return json.loads(p.read_text())
    m = read(c / 'manifest.json')
    outer = read(extracts / 'summary.json')
    prefixes = ('tactix::reduce_apply_refs(', 'tactix::kernel_sparse_collect_root(',
                'tactix::kernel_collect_garbage(')
    results = {}
    for arm in ('candidate', 'previous'):
        binary = str(c / m['arms'][arm]['path'])
        bind(Path(binary), m['arms'][arm]['sha256'])
        p = extracts / (arm + '.samples.txt.xz')
        bind(p, outer['hashes'][str(p.relative_to(root))])
        sites = {int(k): v for k, v in m['call_sites'][arm]['sites'].items()}
        parts = []
        for i, prefix in enumerate(prefixes):
            with lzma.open(p, 'rt') as stream:
                value = h.summarize(scoped(samples(stream), binary, prefixes[:i]),
                    binary, lambda s: s.startswith(prefix), {m['phase_entry']: None},
                    m['phase_caller'], sites)
            if i == 0:
                expected = outer['function_envelopes'][arm]
                assert value['selected_period'] == expected['selected_period']
                assert value['total_period'] == expected['total_period']
                assert value['samples'] == expected['samples']
            else:
                parent = parts[-1]
                assert value['total_period'] == parent['selected_period']
                for phase, branches in parent['branches'].items():
                    expected = sum(n for s, n in branches.items() if s.startswith(prefix))
                    assert value['by_phase'].get(phase, 0) == expected
            assert value['selected_period'] > 0
            assert set(value['by_phase']) == {'no_phase_frame'}
            parts.append(value)
        results[arm] = dict(zip(('reduce', 'sparse', 'gc'), parts))
        print(arm, {name: v['selected_period'] for name,v in results[arm].items()}, flush=True)
    for p in (Path(__file__), reader, root / 'tools/bench/perf_callstack.py'): bind(p)
    with target.open('x') as out:
        json.dump(dict(wave=wave, hashes=hashes, arms=results,
            ratios={name: results['candidate'][name]['selected_period'] /
                         results['previous'][name]['selected_period'] for name in ('reduce','sparse','gc')},
            limitation='One matched profile pair; inclusive scopes overlap. Exact parent/child conservation, not instruction prices, a repetition median, total runtime/RSS improvement or proof acceptance.'), out, indent=2)


if __name__ == '__main__': main()
