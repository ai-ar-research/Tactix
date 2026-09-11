#!/usr/bin/env python3
"""Build the isolated sparse watch filter/forward transaction on retained7144."""
from pathlib import Path

base = Path(__file__).with_name('build-factor-arena-7180.py')
code = base.read_text()
changes = {
    'runtime-factor-arena-7180': 'runtime-gc-forward-7233',
    '20260910-7180-factor-arena-build': '20260911-7233-gc-forward-build',
    'pin-7180-factor-arena-': 'pin-7233-gc-forward-',
    "{'src/rules/factor/factor.cpp', 'src/rules/factor/occurrence_arena.hpp',\n                   'tests/test_factor_occurrence_arena.cpp'}":
        "{'src/kernel/kernel.cpp', 'src/kernel/watch.hpp', 'tests/test_watch_compact_map.cpp', 'tests/test_gc_forward.cpp'}",
    "('test_factor_occurrence_arena', 'test_factor_root_support', 'test_factor', 'test_factor_n1')":
        "('test_watch_compact_map', 'test_gc_forward', 'test_collect_retry', 'test_kernel', 'test_restart_reduce', 'test_reduce_root_lock')",
    'Replace independent per-literal Factor allocations with one eight-byte-record arena. Reuse final peeling counts as initial capacity upper bounds, preserving final connection eligibility and every live encounter order. Dynamic append relocation and stable tail erasure retain caches/degree/tick/proof contracts; size_t offsets avoid a new aggregate32-bit limit. No cofactor-loop rewrite or telemetry ablation included.':
        '7228/7229 locate32.8B sampled periods in GC under Reduce/sparse collection,23.8-24.0B self, including repeated logical-position/garbage predicates and a second watch-forward walk. Mark unretained old records INVALID_REF in the existing retention pass; retained records receive packed Refs. Logical prefix is packed contiguously, so forwarded Ref below packed prefix boundary exactly identifies retained prefix watches. Stable fused filter/forward uses no global array or new allocation. Whole-arena/dense paths unchanged. Independent retained7144 base; normal telemetry and all scheduling/tick/proof contracts retained.',
    'New34 storage layouts compare every operation against vector-of-vectors: mixed growth, cached-ID mutation, stable duplicate deletion, empty erase, reset and fresh literals. Existing Factor support, N1, grid/tick/pose/extension/model tests run in all three suites. Independent exact-parent proofs, DebugASan/UBSan/LSan, BC and complete-Factor/whole-runtime/RSS checks still required.':
        '26240 fused versus old two-pass watch states compare every live word, layout and hole count, including duplicates, mixed order, empty lists, mapped Ref0 and subsequent appends.896 actual collector states compare retained7144 and candidate arena/live-watch/layout/occurrence/eager/OTFS digests across seven cuts,16 garbage masks, aliases, physical dense mode and register preservation. Full Release/BC/trace suites and focused kernel/root-lock/retry tests cover protected reasons and roots. Independent proofs/models, twice-run sanitizers, BC, complete GC/Reduce and repeated runtime/RSS remain required; deterministic states are not fuzz acceptance.',
    'Arena growth invalidates iterators globally; no caller may retain one across any append. Initial upper-bound capacity and abandoned grown slices may inflate RSS; measure, do not assume a memory win. Binary-only scope performs no large-index scan. Kernel/global FlatLists contract unchanged.':
        'Prefix garbage is intentionally retained unless its aliased source is excluded. Every uncopied record must have INVALID_REF before watch filtering; small stale searched cursors must not masquerade as prefix references. Retained replacement cursors, blockers, order and reason/eager/OTFS forwarding must remain exact. Dead watch spare words need not match but live layout and later append behavior must. Sentinel writes may cost more than removed lookups; complete GC/Reduce and whole-runtime/RSS decide. No N1/API/guard/proof vocabulary changes.',
    'patches = {}': """previous_build = root / 'build/pin-7162-vivify-revalidation-release'
objects = sorted((previous_build / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
previous_test = c / 'test_gc_forward_previous'
run('previous-fixture-build', ['g++', '-std=c++20', '-O3', '-fno-exceptions', '-fno-rtti',
    '-I'+str(parent / 'src'), '-I'+str(parent / 'include'),
    str(source / 'tests/test_gc_forward.cpp'), *map(str, objects), '-o', str(previous_test)])
run('previous-test_gc_forward', [str(previous_test)])
expected = (c / 'logs/previous-test_gc_forward.log').read_bytes()
assert b'states=896' in expected and b'PASS' in expected
for kind in ('release', 'bc', 'trace'):
    assert (c / 'logs' / (kind + '-test_gc_forward.log')).read_bytes() == expected, kind
patches = {}""",
}
for before, after in changes.items():
    assert code.count(before) >= 1, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
