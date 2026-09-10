#!/usr/bin/env python3
"""Bind reviewed basic blocks to existing private-propagation samples."""
from pathlib import Path
import json
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from perf_annotation import parse_periods
c = root/'benchmarks/runs/20260910-6888-vivify-propagation-annotation'
s = json.loads((c/'summary.json').read_text())
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
a = parse_periods((c/'annotation.txt').read_text(),s['symbol'])
program = 1243067725865
profile = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/profiles/000-self.txt'
assert f'# Event count (approx.): {program}' in profile.read_text()
regions = {}
for name,begin,end in (
    ('watch_decode_and_blocker_tests',0x412a17,0x412a48),
    ('large_arena_address_and_garbage_test',0x412a48,0x412a66),
    ('large_other_literal_test',0x412a66,0x412a90),
):
    p = a.range_period(begin,end)
    regions[name] = dict(begin=hex(begin),end=hex(end),period=p,
        percent_function=100*p/a.total_period,percent_program=100*p/program)
tree = root/'.claude/worktrees/runtime-reduce-watermark-6730'
paths = [Path(__file__),c/'summary.json',profile,tree/'src/kernel/kernel.cpp',tree/'src/kernel/kernel.hpp',
    tree/'src/kernel/watch.hpp',root/'tools/solvers/variants/kissat-macro-heap-6090/src/proplit.h',
    root/'tools/solvers/variants/kissat-macro-heap-6090/src/fastassign.h',
    root/'tools/solvers/variants/cadical-extdiag/src/vivify.cpp']
out = dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},regions=regions,
    source_review='Rank32 call tree puts private propagate_literal<false,false,true> under Vivify. Tactix splits binary/large paths before the satisfied-blocker test; Kissat consumes the optional tail then performs one blocker test before dispatch. CaDiCaL has separate binary/large sweeps, a trajectory-changing alternative and not a drop-in cost edit. Tactix already hoists values/arena, retains delayed-output storage, preserves native search cursor and candidate-ignore point, and has the stronger bounded true-level reason scan.',
    next_hypothesis='If embedded experiments fail their metric, evaluate the general mixed-watch dispatch shape against Kissat without changing watch layout, propagation order, assignments, proof reasons, ignored-clause placement, effort or scheduler. Inspect generated code before proposing a runtime candidate. Do not treat hot branch samples as proof of misprediction or the arena-load block as removable cost.',
    limitations='Ranges are reviewed basic blocks, not complete dynamic paths or a gain estimate; instruction skid and useful work included. No new solver execution, source edit, raw-profile mutation or native rerun. Main search and Vivify share the template, so any future source change needs both exact gates. No FlatLists or other representation approval inferred.')
with (c/'location-6889.json').open('x') as stream:
    json.dump(out,stream,indent=2);stream.write('\n')
print(json.dumps(regions,indent=2))
