#!/usr/bin/env python3
"""Partition only sparse collectors below the exact Reduce envelopes."""
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
h.self_test()
assert sys.argv[1:] in ([], ['--self-test'])
if sys.argv[1:]: sys.exit(0)
target = root / 'benchmarks/reports/2026-09-11-reduce-collection-7217.json'
assert not target.exists(), 'successful writer frozen'
hashes = {}
def bind(p, expected=None):
    digest = sha(p); assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
outer_path = root / 'benchmarks/reports/2026-09-11-reduce-envelope-7216.json'
outer = json.loads(outer_path.read_text()); bind(outer_path)
for rel, digest in outer['hashes'].items(): bind(root / rel, digest)
campaign = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'
m = json.loads((campaign / 'manifest.json').read_text()); bind(campaign / 'manifest.json')
results = {}
for label in ('candidate-wave0', 'candidate-wave1', 'candidate-wave2', 'native'):
    native = label == 'native'
    if native:
        binary = str(root / 'benchmarks/runs/20260910-7002-largest-gap-release-profiles/pins/kissat')
        p = root / 'benchmarks/runs/20260910-7081-rank35-callstack-cost/kissat.samples.txt.xz'
        entry = 'kissat_reduce'
        selected = 'kissat_sparse_collect'
        entries = {'kissat_probe_initially': 'initial_probe', 'kissat_probe': 'probe', 'kissat_eliminate': 'eliminate'}
        caller = sites = None
    else:
        binary = str(campaign / m['arms']['candidate']['path'])
        wave = int(label[-1])
        p = root / f'benchmarks/runs/20260910-7153-vivify-compact-samples/wave{wave}/candidate.samples.txt.xz'
        entry = 'tactix::reduce_apply_refs('
        selected = 'tactix::kernel_sparse_collect_root('
        entries = {m['phase_entry']: None}; caller = m['phase_caller']
        sites = {int(k): v for k,v in m['call_sites']['candidate']['sites'].items()}
    bind(p)
    with lzma.open(p, 'rt') as stream:
        inside = (s for s in samples(stream) if any(f.dso == binary and f.symbol.startswith(entry) for f in s.frames))
        value = h.summarize(inside, binary, lambda s: s.startswith(selected), entries, caller, sites)
    expected = outer['arms'][label]
    assert value['total_period'] == expected['selected_period']
    for phase, branches in expected['branches'].items():
        selected_period = sum(n for symbol,n in branches.items() if symbol.startswith(selected))
        assert value['by_phase'].get(phase, 0) == selected_period
    results[label] = value
    print(label, value['selected_period'], flush=True)
for p in (Path(__file__), reader, root / 'tools/bench/perf_callstack.py',
          root / '.claude/worktrees/runtime-vivify-compact-7144/src/kernel/kernel.cpp',
          root / 'kissat/src/collect.c'):
    bind(p)
with target.open('x') as stream:
    json.dump(dict(hashes=hashes, arms=results,
        limitation='Nested collector partition only below each retained Reduce envelope. Parent/direct-child/leaf scopes overlap and cannot be summed. Different data representation, inlining and wrapper duties preclude exact cross-solver operation prices. No new execution or source edit.'), stream, indent=2)
