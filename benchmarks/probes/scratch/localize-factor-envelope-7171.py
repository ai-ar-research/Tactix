#!/usr/bin/env python3
"""Separate Factor's own instructions from its immediate sampled descendants."""
from collections import Counter, defaultdict
import json
import lzma
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from perf_callstack import Frame, Sample, samples, phase

def summarize(stream, binary, selected, entries, caller, sites):
    total = count = selected_period = 0
    phases = Counter()
    branches, leaves = defaultdict(Counter), defaultdict(Counter)
    for sample in stream:
        assert sample.period > 0 and sample.frames
        total += sample.period
        count += 1
        matches = [i for i, f in enumerate(sample.frames) if f.dso == binary and selected(f.symbol)]
        if not matches: continue
        index = matches[-1]
        name, _ = phase(sample, binary, entries, caller, sites)
        assert name != 'unresolved_phase_caller'
        selected_period += sample.period
        phases[name] += sample.period
        child = sample.frames[index - 1] if index else None
        key = child.symbol + ' [' + child.dso + ']' if child else '<entry self>'
        branches[name][key] += sample.period
        leaf = sample.frames[0]
        leaves[name][leaf.symbol + ' [' + leaf.dso + ']'] += sample.period
    assert selected_period == sum(phases.values())
    for name, value in phases.items():
        assert sum(branches[name].values()) == sum(leaves[name].values()) == value
    return dict(samples=count, total_period=total, selected_period=selected_period,
                by_phase=dict(phases), branches={k: dict(v) for k, v in branches.items()},
                leaves={k: dict(v) for k, v in leaves.items()})

def self_test():
    f = lambda symbol, dso='solver': Frame(1, symbol, dso)
    data = [Sample(2, (f('entry'),)),
            Sample(3, (f('malloc', 'libc'), f('build'), f('entry'))),
            Sample(5, (f('entry', 'other'),)),
            Sample(7, (f('entry'), f('entry')))]
    r = summarize(data, 'solver', lambda s: s == 'entry', {}, None, None)
    assert r['total_period'] == 17 and r['selected_period'] == 12
    assert r['branches']['no_phase_frame'] == {'<entry self>': 2, 'build [solver]': 3, 'entry [solver]': 7}
    for data in ([Sample(0, (f('entry'),))], [Sample(1, ())]):
        try: summarize(data, 'solver', lambda s: True, {}, None, None)
        except AssertionError: pass
        else: raise AssertionError('bad sample accepted')
    print('Factor partition controls: self, descendant, foreign DSO, recursion, missing phase and bad records PASS')

def main():
    self_test()
    assert sys.argv[1:] in ([], ['--self-test'])
    if sys.argv[1:]: return
    target = root / 'benchmarks/reports/2026-09-10-factor-envelope-7171.json'
    assert not target.exists()
    hashes = {}
    def bind(p, expected=None):
        digest = sha(p)
        assert expected is None or digest == expected, p
        hashes[str(p.relative_to(root))] = digest
    def read(p):
        bind(p)
        return json.loads(p.read_text())
    campaign = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'
    m = read(campaign / 'manifest.json')
    specs = []
    for wave in range(3):
        c = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}'
        summary = read(c / 'summary.json')
        p = c / 'candidate.samples.txt.xz'
        specs.append(dict(label=f'candidate-wave{wave}', binary=campaign / m['arms']['candidate']['path'],
            binary_hash=m['arms']['candidate']['sha256'], stream=p,
            stream_hash=summary['hashes'][str(p.relative_to(root))], expected=summary['arms']['candidate'],
            selected=lambda s: s.startswith('tactix::factor_apply_seeded('),
            entries={m['phase_entry']: None}, caller=m['phase_caller'],
            sites={int(k): v for k, v in m['call_sites']['candidate']['sites'].items()}))
    native_path = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost/lifecycle-summary-7082.json'
    native = read(native_path)
    binary = root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles/pins/kissat'
    p = native_path.parent / 'kissat.samples.txt.xz'
    specs.append(dict(label='native', binary=binary, binary_hash=native['hashes'][str(binary.relative_to(root))],
        stream=p, stream_hash=native['hashes'][str(p.relative_to(root))], expected=native['arms']['kissat'],
        selected=lambda s: s == 'kissat_factor',
        entries={'kissat_probe_initially': 'initial_probe', 'kissat_probe': 'probe', 'kissat_eliminate': 'eliminate'},
        caller=None, sites=None))
    results = {}
    for s in specs:
        bind(s['binary'], s['binary_hash'])
        bind(s['stream'], s['stream_hash'])
        with lzma.open(s['stream'], 'rt') as stream:
            r = summarize(samples(stream), str(s['binary']), s['selected'], s['entries'], s['caller'], s['sites'])
        expected = s['expected']
        assert (r['samples'], r['total_period']) == (expected['samples'], expected['total_period'])
        for name, components in expected['components'].items():
            assert r['by_phase'].get(name, 0) == components.get('factor', 0), (s['label'], name)
        results[s['label']] = r
        print(s['label'], r['by_phase'], flush=True)
    for p in (Path(__file__), root / 'tools/bench/perf_callstack.py',
              root / '.claude/worktrees/runtime-vivify-compact-7144/src/rules/factor/factor.cpp',
              root / 'kissat/src/factor.c'):
        bind(p)
    with target.open('x') as stream:
        json.dump(dict(hashes=hashes, arms=results,
            limitation='Disjoint immediate-child partition within each complete Factor envelope. Child scopes include all their descendants and cannot be added to nested leaves. Compiler inlining and different algorithm responsibilities preclude calling cross-window differences exact operation prices. Missing phase ancestry is retained. No new solver execution or source edit.'), stream, indent=2)

if __name__ == '__main__': main()
