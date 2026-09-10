#!/usr/bin/env python3
"""Bind the redundant binary-only compaction hypothesis to checked samples."""
from pathlib import Path
import json
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from perf_annotation import parse_periods
c = root/'benchmarks/runs/20260910-6900-watch-rebuild-annotation'
target = c/'location-6901.json'
assert not target.exists()
s = json.loads((c/'summary.json').read_text())
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
a = parse_periods((c/'annotation.txt').read_text(),s['symbol'])
profile = root/'benchmarks/runs/20260910-6815-inprocessing-release-profiles/profiles/000-self.txt'
program = 1243067725865
assert f'# Event count (approx.): {program}' in profile.read_text()
p = a.range_period(0x410c98,0x410d34)
tree = root/'.claude/worktrees/runtime-reduce-watermark-6730'
paths = [Path(__file__),c/'summary.json',profile,
    *[tree/rel for rel in ('src/kernel/kernel.cpp','src/kernel/kernel.hpp','src/kernel/watch.hpp',
        'src/rules/vivify/vivify.cpp','src/rules/substitute/substitute.cpp','tests/test_kernel.cpp')],
    root/'tools/solvers/variants/kissat-macro-heap-6090/src/watch.c',
    root/'tools/solvers/variants/cadical-extdiag/src/watch.cpp']
out = dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},
    compaction_hot_loop=dict(begin='0x410c98',end='0x410d34',period=p,
        percent_function=100*p/a.total_period,percent_program=100*p/program),
    diagnosis='Rebuild always compacts all lists to binary-only. Vivify/Substitute callers already disconnected large watches. Existing per-list exact binary population can prove a list has no large entries without inspecting words. However binary-only lists may carry a conservative false binaries-first bit; old compact normalizes it, so a bare skip is not exact. Preserve that normalization with set_layout(size,true), leave all other fields/words/holes unchanged, and retain old compaction for mixed lists. Use per-list has_large rather than relying solely on the global connected flag. No new field/layout/API/guard/scheduler/cost currency.',
    references='Kissat reconnect traverses live arena clauses and appends watches, with no second all-binary scan. CaDiCaL reconnect has distinct binary/large clause passes; not a neutral replacement for Tactix. Preserve native logical clause order, watch selection, root cleanup, proof delta and complete connected-call behavior.',
    gate='Independent6730 candidate only. Actual old/new kernel objects and same-header remaining objects must agree on full watch heads/arena/holes, order-bit normalization, empty/binary/mixed/disconnected/connected states, attached dense view, roots and both flush modes. Add tests; run full independent suites and fixed proofs before B28+C5 and three-wave32/39/38 same-work runtime/memory gates. Reject no useful tradeoff; no repeat/packing/layout tuning. No source fuzz/promotion or FlatLists authority.',
    limitation='1413 sampled self events; this hot-loop range excludes cold compaction blocks and includes useful work on connected mixed lists. Not a whole-function removable-cost ceiling, native unit price or predicted gain. No new solver run.')
with target.open('x') as stream:
    json.dump(out,stream,indent=2);stream.write('\n')
print(json.dumps(out['compaction_hot_loop'],indent=2))
