#!/usr/bin/env python3
"""Partition each exact paired Factor envelope into disjoint direct children."""
from collections import Counter
import importlib.util
import json
import lzma
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import samples

reader = Path(__file__).with_name('localize-factor-envelope-7171.py')
spec = importlib.util.spec_from_file_location('factor_partition', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)

def branches(summary):
    result = Counter()
    for values in summary['branches'].values():
        for key, value in values.items():
            name = key.rsplit(' [', 1)[0]
            result[name] += value
    assert sum(result.values()) == summary['selected_period']
    return result

def fixtures():
    h.self_test()
    r = dict(branches={'probe': {'malloc [/lib/libc.so]': 2, 'entry [candidate]': 3},
                       'no_phase_frame': {'malloc [/lib/libc.so]': 5}}, selected_period=10)
    assert branches(r) == {'malloc': 7, 'entry': 3}
    try: branches(dict(r, selected_period=9))
    except AssertionError: pass
    else: raise AssertionError('lost periods accepted')
    print('Branch merge: phase conservation, DSO-label normalization and lost-period rejection PASS')

def main():
    fixtures()
    if sys.argv[1:] == ['--self-test']: return
    assert len(sys.argv) == 2 and sys.argv[1] in ('0', '1', '2')
    wave = int(sys.argv[1])
    c = root / 'benchmarks/runs/20260910-7188-factor-arena-samples' / f'wave{wave}'
    target = c / 'factor-child-partition-7191.json'
    assert not target.exists()
    if not (c / 'summary.json').exists():
        print('Paired extraction pending; no output created'); return
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p); assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    p = c / 'summary.json'; bind(p); previous = json.loads(p.read_text())
    for rel, digest in previous['hashes'].items(): bind(root / rel, digest)
    campaign = root / 'benchmarks/runs/20260910-7187-factor-arena-profiles'
    p = campaign / 'manifest.json'; bind(p); m = json.loads(p.read_text())
    arms, flat = {}, {}
    for arm in ('candidate', 'previous'):
        binary = campaign / m['arms'][arm]['path']; bind(binary, m['arms'][arm]['sha256'])
        p = c / (arm + '.samples.txt.xz'); bind(p)
        with lzma.open(p, 'rt') as stream:
            result = h.summarize(samples(stream), str(binary),
                lambda s: s.startswith('tactix::factor_apply_seeded('),
                {m['phase_entry']: None}, m['phase_caller'],
                {int(k): v for k,v in m['call_sites'][arm]['sites'].items()})
        expected = previous['function_envelopes'][arm]
        for key in ('samples', 'total_period', 'selected_period', 'by_phase'):
            assert result[key] == expected[key], (arm, key)
        arms[arm], flat[arm] = result, branches(result)
    delta = {name: flat['candidate'][name] - flat['previous'][name]
             for name in flat['candidate'].keys() | flat['previous'].keys()}
    assert sum(delta.values()) == arms['candidate']['selected_period'] - arms['previous']['selected_period']
    for p in (Path(__file__), reader, root / 'tools/bench/perf_callstack.py'): bind(p)
    with target.open('x') as out:
        json.dump(dict(wave=wave, hashes=hashes, arms=arms,
            direct_children={arm: dict(v.most_common()) for arm,v in flat.items()},
            candidate_minus_previous=dict(sorted(delta.items(), key=lambda kv: kv[1])),
            complete_factor_ratio=previous['complete_factor_ratio'],
            limitation='One paired sampled envelope. Direct-child categories are disjoint and include descendants; nested leaves are not additive. Inlining can move work between children/self, so interpret the complete envelope first. No individual instruction price, final proof acceptance, RSS or three-wave conclusion.'), out, indent=2)
    print(target)
    for arm in arms: print(arm, arms[arm]['selected_period'], flat[arm].most_common(8))

if __name__ == '__main__': main()
