#!/usr/bin/env python3
"""Bind the approved full-width experiment, own-arm gates and failed controls."""
from pathlib import Path
import json
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6869-subsolver-binary-build'
b = json.loads((c/'manifest.json').read_text())
tree = root/b['source']; parent = root/b['parent']
changed = []
for rel,digest in b['files'].items():
    assert sha(tree/rel) == digest
    if not (parent/rel).exists() or sha(parent/rel) != digest: changed.append(rel)
assert set(changed) == {'src/engine/subsolver.cpp','src/engine/subsolver.hpp','tests/test_subsolver_binary.cpp'}
header = (tree/'src/engine/subsolver.hpp').read_text()
assert 'using SubRef = uint32_t;' in header and 'using SubLit = uint32_t;' in header
assert 'static_assert(sizeof(SubWatch) == 12);' in header
cpp = (tree/'src/engine/subsolver.cpp').read_text()
assert cpp.count('ws.size() * KITTEN_REFERENCE_WATCH_BYTES') == 2
assert 'constexpr uint32_t KITTEN_REFERENCE_WATCH_BYTES = 2 * sizeof(uint32_t);' in cpp
assert 'ws.size() * sizeof(SubWatch)' not in cpp
assert 'search_mode_ == SubSearchMode::Kitten && w.binary' in cpp
assert cpp.count('watches_[c[1]].push_back({other, w.ref, false});') == 2
assert 'const bool binary = clause_size(ref) == 2;' in cpp
for number in (6874,6881):
    control = json.loads((c/f'control-{number}.json').read_text())
    assert control['outputs']['previous'] == control['outputs']['candidate']
    for rel,digest in control['hashes'].items(): assert sha(root/rel) == digest,rel
failed = c/'logs/core-own-arm-previous-link.json'
f = json.loads(failed.read_text())
assert f['exit_code'] != 0 and 'redefinition' in f['stderr']
assert not (c/'logs/core-own-arm-candidate-link.json').exists()
paths = [Path(__file__),c/'manifest.json',c/'control-6870.json',c/'control-6874.json',c/'control-6881.json',failed,
    root/'benchmarks/reports/2026-09-10-subsolver-binary-design.md',
    root/'benchmarks/runs/20260910-6872-subsolver-binary-proofs/summary.json',
    root/'benchmarks/probes/scratch/test-subsolver-core-binary-6878.cpp',
    root/'benchmarks/probes/scratch/test-subsolver-core-binary-6880.cpp',
    root/'tools/solvers/variants/kissat-macro-heap-6090/src/kitten.c',
    root/'tools/solvers/variants/cadical-extdiag/src/kitten.c']
out = dict(hashes={str(p.relative_to(root)):sha(p) for p in paths},changed=changed,
    approval='User Approve applies to isolated12-byte full-width binary-watch prototype, not adoption, packing, FlatLists, policy changes or promotion.',
    layout='Blocking literal and reference stay uint32_t; bool gives12-byte watch (+50% storage per watch). Native8-byte katch remains logical effort currency at propagation and flip. Runtime and RSS are independent measured acceptance dimensions.',
    metadata_lifetime='Attach derives size==2 for original/learned clauses and rebuilt cores. Binary watches never move and blocker is always the other literal. Generic survivors retain tags, actual large moves set false, vector shuffles copy full records. Fixed-mode orientation and model-flip dereference behavior preserved; fast path Kitten-only. Root assign/proof reason and unread conflict suffix preserved.',
    controls='113/113/112 complete own-source suites.6874 complete own-arm headers/objects match720 configurations/12960 incremental solves and existing SubSolver/Sweep suites.6881 adds direct clausal-core rebuild/shuffle/LRAT checks after initial6879 compile failure.120 fixed external DRAT/LRAT checks pass. No fuzz acceptance.',
    failed_or_limited_controls=[
        dict(id=6870,classification='Passing mixed-header link is not layout-safe acceptance; logs preserved.6874 replaces the correctness control with complete own-arm objects.'),
        dict(id=6879,classification='Nested fixture main macro was undefined by its include, causing compile-time duplicate main before any solver. Failed fixture6878/log preserved.6880 includes existing base test directly and copies shared fixed oracles;6881 is fresh successful control.'),
    ],
    limitation='Unadopted/unfuzzed. Exact B/C work and certificates, three matched timing waves, host memory and full50 gates remain separate. No global CDCL/scheduling parity claimed. Production and all older pins unchanged.')
with (c/'scope-6875.json').open('x') as stream:
    json.dump(out,stream,indent=2); stream.write('\n')
print('PASS approved scope, own-arm controls and preserved failed/limited diagnostics')
