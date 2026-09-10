#!/usr/bin/env python3
"""Bind the reviewed XOR membership range; sample location is not removable time."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
source = root/'benchmarks/runs/20260910-6948-congruence-extraction-annotation/summary.json'
s = json.loads(source.read_text())
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest
lo, hi = 0x4605fe, 0x460879
assert {hex(lo), hex(hi)} <= {r['address'] for r in s['rows']}
rows = [r for r in s['rows'] if lo <= int(r['address'], 16) <= hi]
paths = [source, Path(__file__),
         root/'tools/solvers/variants/kissat-macro-heap-6090/src/congruence.c',
         root/'tools/solvers/variants/cadical-extdiag/src/congruence.cpp']
result = dict(hashes={str(p.relative_to(root)): sha(p) for p in paths},
    range=[hex(lo), hex(hi)], rows=rows, period=sum(r['period'] for r in rows),
    function_period=s['total_period'], program_period=523126187061,
    mapping='Inlined XOR membership: first-literal directory, lower_bound over compact records, key comparison and parity-loop continuation. Includes larger arities and necessary work. ITE conditional pair scan at461cd0 is a separate cost.',
    decision='Test reuse of existing ternary_presence for distinct-variable size3 XOR parity completeness. Preserve upfront combos ticks, gate/discovery order, unfinished-index rejection and size4/5 fallback. No new index/layout/API or strategy change; no claim the sampled range is removable.')
target = root/'benchmarks/runs/20260910-6948-congruence-extraction-annotation/location-6949.json'
with target.open('x') as out:
    json.dump(result, out, indent=2); out.write('\n')
print(result['period'], result['period']/result['program_period'])
