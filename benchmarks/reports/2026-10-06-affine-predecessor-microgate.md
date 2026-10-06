# Bounded GF(2) predecessor microgate

This internal matrix increment retains one complete predecessor on a successful
monotone extension. An exact shorter snapshot can restore it by move, preserving
the original row ordering, source ordinals, implication/conflict and reason.
Same-length reassignment, nonmatching backtrack, prior partial reduction and
population-cap declines retain the authenticated full-rebuild path. Restoration
consumes its checkpoint rather than accumulating a trail history.
An equal-length predecessor comparison is charged before testing its contents;
a nonmatching snapshot pays that comparison work before the normal reduction.

Current and predecessor together must fit `maximum_rows` and
`maximum_assignments`. Each row still satisfies the declared width/source caps.
The current surviving-variable union is a subset of its complete predecessor on
extension, so their union stays within `maximum_variables`. Immutable admitted
rows remain separate as before. These are logical population bounds, not a new
byte-level allocation guarantee; temporary reduction allocation remains bounded
separately by the existing reduction limits.

The fixed two-equation collision fixture restores a two-assignment implication
in 3 charged units (`1 + trail size`), compared with 17 units for rebuilding its
identical rows/event/reason from admission. This is an operation-count microgate,
not solver timing or target recovery evidence. Tests also cover row-cap and
trail-cap cache declines, interrupted restore atomicity, deeper backtrack,
exact event authentication and forged-source rejection. The existing fixed
exhaustive four-variable prefix equivalence and five-variable reason oracles
remain active. No solver, fuzz or benchmark campaign was launched.

Validation uses direct standalone compilation of `tests/test_affine_matrix.cpp`
and `src/rules/semantic_extension/affine_matrix.cpp` in
`build/agent-gf2-incremental-20261006`, with C++20 and `-Wall -Wextra -Werror`:
Debug passes; ASan/UBSan passes with `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer
with leak detection enabled fails because this environment uses ptrace; that
failure supplies no leak-check result. `git diff --check` passes.

General touched-row scheduling, local pivot repair and useful during-CDCL target
evidence remain open. Checkpoints at full mutable row/trail caps deliberately
decline caching. Full Debug, Claude-owned both-policy fuzz and applicable
proof/model and regression campaigns remain promotion gates. Existing raw
`Backtrack` classification is preserved; a restore reports zero conditioned
assignments and does not imply a Gaussian rebuild merely because it is a
backtrack call.
