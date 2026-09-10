#!/usr/bin/env python3
"""Locate existing Factor samples without treating useful work as removable."""
from pathlib import Path
import json
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from perf_annotation import parse_periods

c = root/'benchmarks/runs/20260910-6920-factor-cost-annotation'
target = c/'location-6921.json'
assert not target.exists(), 'successful writer frozen'
s = json.loads((c/'summary.json').read_text())
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest, rel
a = parse_periods((c/'annotation.txt').read_text(), s['symbol'])
p = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles'
program = 1243067725865
assert f'# Event count (approx.): {program}' in (p/'profiles/000-self.txt').read_text()
current = json.loads((p/'results/000.json').read_text())
native = json.loads((p/'results/001.json').read_text())
assert classify(current, True)['checked'] and classify(native, True)['checked']
assert 'c factor      7357 factorings, 19687122 large remainders offered, 428217051 ticks' in current['solver_statistics']
assert 'c timing        Factor               14406 ms (  4.3%) in 86 applications' in current['solver_statistics']
for expected in ('c factored:                              7357                2 %  variables',
                 'c factorizations:                          86            53913    interval',
                 'c factor_ticks:                     428217051 '):
    assert expected in native['solver_statistics'], expected
regions = {}
for name, begin, end, interpretation in (
    ('large_candidate_scan', 0x47fef0, 0x480046,
     'Factor index iteration, reserved/size rejection, quotient membership, unique candidate and marks. Mirrors useful native next_factor large-clause work; includes counter/touched calls and their return sites.'),
    ('binary_candidate_count', 0x480198, 0x4801f9,
     'Packed binary traversal, boundary/FACTOR/active checks and cofactor count. The zero-count path branches to the separately listed touched-vector call.'),
    ('first_touch_return', 0x4814bd, 0x4814e9,
     'First-discovery touched.push_back call and cofactor pointer/count reload before increment. A candidate for compile-only same-state dataflow inspection, not proof that push_back or allocation causes every sample.'),
):
    period = a.range_period(begin, end)
    regions[name] = dict(begin=hex(begin), end=hex(end), period=period,
                         percent_function=100*period/a.total_period,
                         percent_program=100*period/program, interpretation=interpretation)
tree = root/'.claude/worktrees/runtime-reduce-watermark-6730'
paths = [Path(__file__), c/'summary.json', p/'profiles/000-self.txt',
         p/'results/000.json', p/'results/001.json',
         tree/'src/rules/factor/factor.cpp', tree/'src/rules/registry.cpp',
         tree/'src/policies/default_policy.cpp',
         root/'tools/solvers/variants/kissat-macro-heap-6090/src/factor.c',
         root/'tools/solvers/variants/kissat-macro-heap-6090/src/profile.c']
out = dict(hashes={str(path.relative_to(root)):sha(path) for path in paths},
    regions=regions, exact_reported_factor_work=dict(applications=86, accepted=7357, ticks=428217051),
    clock_limitation='Tactix Factor14406ms is steady-clock rule time; native Factor8.80s is nested process-CPU time. Equal reported counts do not prove full internal-work identity or a precise 1.637x unit-cost ratio. Do not sum nested native timers or subtract them from wall time.',
    preserved='Already optimized: open-addressed remainder intern table, lazy per-watch remainder cache, byte FACTOR/QUOTIENT/NOUNTED marks, cached byte activity, reserved clause-header marks, borrowed initial binary watches and packed binary scan. FactorIndex remains full-reference8B entries in per-literal vectors. No FlatLists or reference narrowing authority.',
    next_action='After closing6902, inspect generated dataflow around first-touch count and clause rejection in a compile-only isolated6730 control. First compare reference mechanisms and establish redundant physical work; keep predicate order, discovery/tie order, counters, API and budget boundaries exact. Do not implement a representation redesign or reopen shelved Rank21.',
    test_consideration='No solver or maintained parser changed. Existing perf_annotation tests own range accounting, strict published-result classification owns checker status; this helper verifies exact pinned counts and source hashes. Any subsequent code change needs an explicit unit-test decision and full-state old/new controls.',
    limitation='Existing1101 self samples only; sampled return sites may carry skid/callee effects. Noncontiguous regions do not cover all Factor work. Useful work is not a removable-cost ceiling or predicted speedup. No new solver run, source change, adoption, fuzz or reference refresh.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2)
    stream.write('\n')
print(json.dumps(regions, indent=2))
