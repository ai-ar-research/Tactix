#!/usr/bin/env python3
"""Bind the no-policy-change scope and the corrected production default evidence."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6822-vivify-snapshot-build'
target = c/'scope-6834.json'
assert not target.exists()
b = json.loads((c/'manifest.json').read_text())
source = root/b['source']; parent = root/b['parent']
changed = []
for rel, digest in b['files'].items():
    assert sha(source/rel) == digest, rel
    if not (parent/rel).exists() or sha(parent/rel) != digest: changed.append(rel)
assert set(changed) == {'src/rules/vivify/vivify.cpp', 'tests/test_vivify_snapshot.cpp', 'tests/test_vivify_count_cache.cpp'}
test_before = (parent/'tests/test_vivify_count_cache.cpp').read_text()
test_after = (source/'tests/test_vivify_count_cache.cpp').read_text()
assert test_after == test_before[:-2] + '  return 0;\n}\n'
code = (source/'src/rules/vivify/vivify.cpp').read_text()
old = (parent/'src/rules/vivify/vivify.cpp').read_text()
assert code[code.index('static bool in_scope'):code.index('struct VivifyCand')] == old[old.index('static bool in_scope'):old.index('struct VivifyCand')]
assert code[code.index('void vivify_candidates'): ] == old[old.index('void vivify_candidates'): ]
assert 'const bool count_only = k.vivify_scope == 2 && !k.vivify_flush;' in code
bindings = {}
for tree in (root, parent, source):
    for rel in ('src/kernel/kernel.hpp', 'src/main.cpp', 'src/policies/default_policy.cpp'):
        p = tree/rel; bindings[str(p.relative_to(root))] = sha(p)
    assert 'bool vivify_flush = false;' in (tree/'src/kernel/kernel.hpp').read_text()
    assert 'if (const char *vf = getenv("TACTIX_VIVIFY_FLUSH"))\n    kernel.vivify_flush = (*vf != \'0\');' in (tree/'src/main.cpp').read_text()
history = subprocess.check_output(['git', 'show', '3fdaf955', '--', 'src/kernel/kernel.hpp'], cwd=root, text=True)
assert '-  bool vivify_flush = true;' in history and '+  bool vivify_flush = false;' in history
for p in (Path(__file__), c/'manifest.json', root/'docs/scheduling-parity-ledger.md',
          root/'tools/solvers/variants/kissat-macro-heap-6090/src/vivify.c',
          root/'tools/solvers/variants/cadical-extdiag/src/vivify.cpp',
          root/'benchmarks/runs/20260910-6824-vivify-snapshot-control/control-6824.json',
          root/'benchmarks/runs/20260910-6827-vivify-snapshot-proofs/summary.json'):
    bindings[str(p.relative_to(root))] = sha(p)
out = dict(hashes=bindings, changed=changed, historical_default_change=history,
    implementation='A pure-irredundant non-flushing schedule needs no literal pose or prefix key. Each appended reference is stable until both root passes finish: first-pass replacements retain false priority; second pass visits true priority only; unit/refutation returns precede sorting. Compute the same final maximum count from surviving clauses. Keep all snapshots for mixed scopes and flushing. No reconstruction, proof delta, guard, API, policy, budget, CDCL or kernel-layout change.',
    reference='Kissat init_countref reads the maximum occurrence count directly from the clause for COUNTREF_COUNTS1. CaDiCaL also derives compact occurrence ranks directly from clauses; its tie policy is not copied over the Kissat tie-break.',
    documentation='Correct SP-VIVIFY-3 stale DEFAULT ON label to observed NON-DEFAULT ARM. Production default changed true to false in3fdaf955 on2026-08-12, not this experiment. C3 calculus and implemented guards reviewed unchanged; no new semantic invariant or hidden action restriction.',
    tests='432 baseline+216 new actual old/new state/model/proof-digest cases and144 ordering cases;113/113/112 suites;48 checked fixed proofs. Existing option-coverage checker confirms418 reference options accounted. No fuzz acceptance or runtime gain yet.',
    limitation='Cold snapshot capacity may fall, but prior redundant schedules retain scratch capacity and arena rereads can offset avoided copies. Only matched repeated timing can decide benefit; no speedup inferred from source or microfixtures.')
with target.open('x') as stream:
    json.dump(out, stream, indent=2); stream.write('\n')
print(target)
