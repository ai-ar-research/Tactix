#!/usr/bin/env python3
"""Build an isolated, work-preserving AND membership/position loop fusion."""
from pathlib import Path

base = Path(__file__).with_name('build-factor-arena-7180.py')
code = base.read_text()
replacements = {
    'runtime-factor-arena-7180': 'runtime-congruence-fused-7196',
    '20260910-7180-factor-arena-build': '20260911-7196-congruence-fused-build',
    'pin-7180-factor-arena-': 'pin-7196-congruence-fused-',
    "{'src/rules/factor/factor.cpp', 'src/rules/factor/occurrence_arena.hpp',\n                   'tests/test_factor_occurrence_arena.cpp'}":
        "{'src/rules/congruence/congruence.cpp', 'tests/test_congruence_fused_extract.cpp'}",
    "('test_factor_occurrence_arena', 'test_factor_root_support', 'test_factor', 'test_factor_n1')":
        "('test_congruence_fused_extract', 'test_congruence_index_cleanup', 'test_congruence', 'test_congruence_empty_gates')",
    'Replace independent per-literal Factor allocations with one eight-byte-record arena. Reuse final peeling counts as initial capacity upper bounds, preserving final connection eligibility and every live encounter order. Dynamic append relocation and stable tail erasure retain caches/degree/tick/proof contracts; size_t offsets avoid a new aggregate32-bit limit. No cofactor-loop rewrite or telemetry ablation included.':
        '7193-7195 localize the dominant dense AND extraction loop to repeated output-position scans. Fuse position discovery into binary-support membership verification so failed candidates stop early; retain first duplicate-output position, stale rejection, exact tags and clause-major order, logical ticks and budgets. Base7144 independently of7180. No arity/degree filtering, scheduling or telemetry change.',
    'New34 storage layouts compare every operation against vector-of-vectors: mixed growth, cached-ID mutation, stable duplicate deletion, empty erase, reset and fresh literals. Existing Factor support, N1, grid/tick/pose/extension/model tests run in all three suites. Independent exact-parent proofs, DebugASan/UBSan/LSan, BC and complete-Factor/whole-runtime/RSS checks still required.':
        'New648 deterministic dense AND scenarios twice compare ordered gates, first positions, LRAT IDs, read-only formula/trail/watches and exact ticks against independent direct-watch membership. Widths3/8/65; first/middle/last outputs, missing supports, fully supported stale entries, duplicate outputs/occurrences, garbage/redundant clauses, polarity, root values and three proof modes. Full suites plus existing Congruence guard/N1/state/model/proof fixtures. Parent-library replay, sanitizers, independent proofs, BC and repeated complete-Congruence/runtime/RSS checks required; source fuzz remains open.',
    'Arena growth invalidates iterators globally; no caller may retain one across any append. Initial upper-bound capacity and abandoned grown slices may inflate RSS; measure, do not assume a memory win. Binary-only scope performs no large-index scan. Kernel/global FlatLists contract unchanged.':
        'A stale occurrence must never become a gate even with all partner bits set. Duplicate output literals retain their first position and skip all equal outputs. Partner marking can begin on a stale record earlier than before but must be cleared per output. No state/layout/allocation or abstract rule contract change; measured hot-PC attribution is not an intrinsic instruction price.',
}
for before, after in replacements.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})
