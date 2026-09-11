#!/usr/bin/env python3
"""Build isolated exact root-true binary retirement against retained7144."""
from pathlib import Path
base = Path(__file__).with_name('build-factor-arena-7180.py')
code = base.read_text()
changes = {
    'runtime-factor-arena-7180': 'runtime-root-binary-7218',
    '20260910-7180-factor-arena-build': '20260911-7218-root-binary-build',
    'pin-7180-factor-arena-': 'pin-7218-root-binary-',
    "{'src/rules/factor/factor.cpp', 'src/rules/factor/occurrence_arena.hpp',\n                   'tests/test_factor_occurrence_arena.cpp'}":
        "{'src/kernel/kernel.cpp', 'tests/test_root_binary_collect.cpp'}",
    "('test_factor_occurrence_arena', 'test_factor_root_support', 'test_factor', 'test_factor_n1')":
        "('test_root_binary_collect', 'test_collect_retry', 'test_kernel', 'test_restart_reduce')",
    'Replace independent per-literal Factor allocations with one eight-byte-record arena. Reuse final peeling counts as initial capacity upper bounds, preserving final connection eligibility and every live encounter order. Dynamic append relocation and stable tail erasure retain caches/degree/tick/proof contracts; size_t offsets avoid a new aggregate32-bit limit. No cofactor-loop rewrite or telemetry ablation included.':
        '7216/7217 locate49.5B sampled periods in the Reduce sparse collector, including14B wrapper self. Visit only root-true watch lists to discover root-satisfied binaries; retain old import-rank orientation, every duplicate copy, lexical deletion order, proof bytes and retry/reason effects. Remove the all-literal rank vector. No lifecycle, policy, physical arena, garbage copy or metadata-scan change; independent7144 base.',
    'New34 storage layouts compare every operation against vector-of-vectors: mixed growth, cached-ID mutation, stable duplicate deletion, empty erase, reset and fresh literals. Existing Factor support, N1, grid/tick/pose/extension/model tests run in all three suites. Independent exact-parent proofs, DebugASan/UBSan/LSan, BC and complete-Factor/whole-runtime/RSS checks still required.':
        'New5400 deterministic old/new primitive cases, twice each, compare exact arena/watch order, binary IDs, trail/reasons, retry/removal advice and counts, with no proof and three proof formats. Cases cover five endpoint assignment states, identity/reverse/unrecorded import ranks,1-3 copies, absent/live/garbage large watches and root-after-nonroot trails. Oracle is exact retained7144 function text with only its name changed. Full Release/BC/trace and focused suites; independent valid proofs/models, sanitizers, BC and repeated complete-collector/runtime/RSS gates remain required.',
    'Arena growth invalidates iterators globally; no caller may retain one across any append. Initial upper-bound capacity and abandoned grown slices may inflate RSS; measure, do not assume a memory win. Binary-only scope performs no large-index scan. Kernel/global FlatLists contract unchanged.':
        'A pair with only the higher-ranked endpoint root-true must still emit in lower-rank orientation; a pair with both true must emit only once per copy. Semantic root levels, not current search truth, select lists. Proof/deletion ordering and learned/native advice remain unchanged. Hot-PC attribution is not an intrinsic price; retain N1/N2 and all established closures.',
    'patches = {}': """original = (parent / 'src/kernel/kernel.cpp').read_text()
start = original.index('RootCollectOutcome kernel_sparse_collect_root(')
end = original.index('// --- propagation engine', start)
expected = original[start:end].strip().replace('kernel_sparse_collect_root', 'reference_collect')
fixture = (source / 'tests/test_root_binary_collect.cpp').read_text()
start = fixture.index('RootCollectOutcome reference_collect(')
end = fixture.index('static void setup(', start)
assert fixture[start:end].strip() == expected, 'old oracle changed beyond its name'
patches = {}""",
}
for before, after in changes.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
