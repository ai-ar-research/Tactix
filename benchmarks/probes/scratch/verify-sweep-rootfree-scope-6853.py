#!/usr/bin/env python3
"""Bind a representation-only Sweep change to unchanged scheduling and CDCL."""
from pathlib import Path
import json
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6838-sweep-rootfree-build'
b = json.loads((c/'manifest.json').read_text())
source = root/b['source']; parent = root/b['parent']
changed = []
for rel,digest in b['files'].items():
    assert sha(source/rel) == digest
    if not (parent/rel).exists() or sha(parent/rel) != digest: changed.append(rel)
assert set(changed) == {'src/rules/sweep/sweep.cpp','tests/test_sweep_rootfree.cpp'}
old = (parent/'src/rules/sweep/sweep.cpp').read_text()
new = (source/'src/rules/sweep/sweep.cpp').read_text()
begin = 'void env_add_clause('
end = '\n} // namespace\n\nRuleEligibility sweep_guard'
assert old[:old.index(begin)] == new[:new.index(begin)]
assert old[old.index(end):] == new[new.index(end):]
assert new.count('env_add_clause(') == old.count('env_add_clause(') == 3
bindings = {}
for p in (Path(__file__),c/'manifest.json',source/'src/rules/sweep/sweep.cpp',parent/'src/rules/sweep/sweep.cpp',
    source/'src/rules/sweep/sweep.hpp',root/'docs/calculus.md',root/'docs/implementation-notes.md',
    root/'tools/solvers/variants/kissat-macro-heap-6090/src/sweep.c',root/'tools/solvers/variants/cadical-extdiag/src/sweep.cpp',
    root/'benchmarks/runs/20260910-6843-sweep-rootfree-control/control-6843.json',
    root/'benchmarks/runs/20260910-6845-sweep-rootfree-proofs/summary.json'):
    bindings[str(p.relative_to(root))] = sha(p)
with (c/'scope-6853.json').open('x') as stream:
    json.dump(dict(hashes=bindings,changed=changed,
        scope='Only env_add_clause implementation changes. All guard/instances/apply, emitters, budgets, scheduler, CDCL, Kernel and public headers are byte-identical to6730. Scratch contents/capacity may differ; encoded semantic state, order and meters may not.',
        safety='All callers supply main-arena clause or stack binary pair. env_add modifies only SweepEnv and stats, never Kernel arena or its input pair. Root-free clauses need no filtering/root-unit ledger. First false literal switches to the original proof-aware filter; strip-off unit insertion stays unchanged. No FlatLists/layout/compact-mapping change.',
        reference='Kissat and CaDiCaL both filter main clauses into a temporary clause stack and import into their embedded solver, with BFS order and structural caps. Tactix has an additional environment copy for its proof mapping. This patch removes that extra temporary copy on root-free inputs, not a claimed identical native fast path.',
        documentation='V5 calculus and implemented guards reviewed unchanged: no semantic rule, proof delta or policy contract revision required. Existing plan already requires scheduling/CDCL parity and unit-test consideration for every change.',
        limitation='Tests establish preservation against6730, not global reference parity or a performance gain.120 fixed proofs checked;B/C and repeated timing remain separate. Current-source fuzz and memory acceptance remain open.'),stream,indent=2)
    stream.write('\n')
print('Scope6853 PASS')
