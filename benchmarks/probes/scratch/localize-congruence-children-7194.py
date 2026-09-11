#!/usr/bin/env python3
"""Resolve large Congruence children only inside the complete rule envelope."""
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
spec = importlib.util.spec_from_file_location('partition', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h)
assert sys.argv[1:] in ([], ['--self-test'])
h.self_test()
if sys.argv[1:]: sys.exit(0)
target = root / 'benchmarks/reports/2026-09-11-congruence-children-7194.json'
assert not target.exists()
hashes = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
parent = root / 'benchmarks/reports/2026-09-11-congruence-envelope-7193.json'
outer = json.loads(parent.read_text()); bind(parent)
for rel, digest in outer['hashes'].items(): bind(root / rel, digest)
campaign = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'
p = campaign / 'manifest.json'; m = json.loads(p.read_text()); bind(p)
binary = str(campaign / m['arms']['candidate']['path'])
selectors = {
    'extract': 'tactix::extract_gates(',
    'canonicalize_binaries': 'tactix::kernel_canonicalize_binary_watches(',
    'forward': 'unsigned long tactix::(anonymous namespace)::forward_subsume_congruent_clauses<',
}
results = {}
for wave in range(3):
    p = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}/candidate.samples.txt.xz'; bind(p)
    data = outer['arms'][f'candidate-wave{wave}']
    selected = {}
    for name, prefix in selectors.items():
        with lzma.open(p, 'rt') as stream:
            inside = (s for s in samples(stream) if any(f.dso == binary and f.symbol.startswith('tactix::congruence_apply(') for f in s.frames))
            value = h.summarize(inside, binary, lambda s: s.startswith(prefix),
                {m['phase_entry']: None}, m['phase_caller'],
                {int(k): v for k,v in m['call_sites']['candidate']['sites'].items()})
        assert value['total_period'] == data['selected_period']
        for phase, branches in data['branches'].items():
            expected = sum(n for symbol,n in branches.items() if symbol.startswith(prefix))
            assert value['by_phase'].get(phase, 0) == expected, (wave, name, phase)
        selected[name] = value
    results[f'wave{wave}'] = selected
    print(wave, {name: value['selected_period'] for name,value in selected.items()}, flush=True)
for p in (Path(__file__), reader, root / 'tools/bench/perf_callstack.py'): bind(p)
with target.open('x') as out:
    json.dump(dict(hashes=hashes, waves=results,
        limitation='Nested direct-child partitions of the three complete retained7144 Congruence envelopes. Parent/child/leaf figures overlap; do not add across these levels. Exact source/binary/sample hashes and outer-envelope conservation required. Native inlining differs; no matched-operation price, solver change or new campaign.'), out, indent=2)
