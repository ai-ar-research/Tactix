#!/usr/bin/env python3
"""Build storage scaffolding and require unchanged retained solver binaries."""
from pathlib import Path
base = Path(__file__).with_name('build-factor-arena-7180.py')
code = base.read_text()
for before, after in {
    'runtime-factor-arena-7180': 'runtime-single-occurrence-7249',
    '20260910-7180-factor-arena-build': '20260911-7249-single-occurrence-build',
    'pin-7180-factor-arena-': 'pin-7249-single-occurrence-',
    "{'src/rules/factor/factor.cpp', 'src/rules/factor/occurrence_arena.hpp',\n                   'tests/test_factor_occurrence_arena.cpp'}":
        "{'src/kernel/connected_occurrence.hpp', 'tests/test_connected_occurrence.cpp'}",
    "('test_factor_occurrence_arena', 'test_factor_root_support', 'test_factor', 'test_factor_n1')":
        "('test_connected_occurrence', 'test_kernel', 'test_bve', 'test_strengthen')",
    'patches = {}':
        "assert binaries['build/pin-7249-single-occurrence-release/tactix'] == sha(root / 'build/pin-7162-vivify-revalidation-release/tactix'), 'unwired scaffolding changed solver binary'\npatches = {}",
    'Replace independent per-literal Factor allocations with one eight-byte-record arena. Reuse final peeling counts as initial capacity upper bounds, preserving final connection eligibility and every live encounter order. Dynamic append relocation and stable tail erasure retain caches/degree/tick/proof contracts; size_t offsets avoid a new aggregate32-bit limit. No cofactor-loop rewrite or telemetry ablation included.':
        'Stage1 storage scaffold only: one owning mixed occurrence stream and exact cached large populations with allocation-free filtered forward ranges. Intended Eliminate cutover removes its duplicate large payload while preserving every entry, duplicate, stale membership and mixed order. Probe direct-large and partial-forward images remain separate migration obligations. No Kernel consumer uses this component yet; retained Release binary must remain byte-identical. Complete lifecycle/RSS gain is unproven.',
    'New34 storage layouts compare every operation against vector-of-vectors: mixed growth, cached-ID mutation, stable duplicate deletion, empty erase, reset and fresh literals. Existing Factor support, N1, grid/tick/pose/extension/model tests run in all three suites. Independent exact-parent proofs, DebugASan/UBSan/LSan, BC and complete-Factor/whole-runtime/RSS checks still required.':
        '21845 mixed sequences,16 stable filters each, every-position erase/replacement, first duplicate deletion, filtered iteration and exact O(1) counts against independent vectors.300000 interleaved pushes force global arena growth, then explicit75-percent-hole packing; fresh literals, reset/release/rebuild and repeated independent owners tested. Full source suites plus kernel/BVE/Strengthen fixed tests. Sanitizers required. Later consumer cutover still requires complete state, proof/model, BC, repetitions, full50 and Claude-owned fuzz; scaffold is not a solver repair.',
    'Arena growth invalidates iterators globally; no caller may retain one across any append. Initial upper-bound capacity and abandoned grown slices may inflate RSS; measure, do not assume a memory win. Binary-only scope performs no large-index scan. Kernel/global FlatLists contract unchanged.':
        'Filtered large iteration scans binary entries that the old contiguous mirror skipped; memory savings may trade against scan cost. Cached counts must follow every mutation. Ranges expire at append/rebuild/packing and changed-list mutation. Existing31-bit payload/FlatLists capacity limits are preserved, not broadened. Exact solver fingerprints and complete lifecycle/RSS measurements must decide the future cutover.',
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
