# Symmetry-use bookkeeping (observation only)

Plan §9.5 defines a symmetry run as fruitful only when search uses its output.
This change adds per-run provenance tags to symmetry-breaking output and counts
how search uses it. It makes no solver decision. Final code: commits `66f054cde`,
`100764994`, `1ebdf1a5c` and `82f85c367`, plus a comment-only correction committed with this report. The measured binary is candidate c2's
source with the same patch (`benchmarks/runs/20261007-symuse-build/symuse-v9.patch`),
in `build/pin-symuse-c2-rel-v9`.

## Design

**Tagging.** `substitution_redundancy_apply_detailed_impl` is the single
publication point for every SR application. After it publishes a clause whose
producer is `Orbitopal` or `GraphSymmetry`, one hook calls
`kernel_symuse_tag(k, kept, size, committed_id, unique_literal_amo)`. The hook
covers FixAndCut units and cuts, generator fixing, orbitopal prefixes and ULC
AMO helper binaries. Helper binaries carry a HELPER bit. Explicit, general and
Tseitin-cycle SR applications are not tagged. The tag is `(run + 1) | HELPER`.
Startup is run 0, and `SymmetryUse::run` holds the tag for later restart runs.

**Representation.** There is no spare clause-header bit: `glue` takes bits 12
to 31, and SR large clauses enter as redundant glue-0 clauses whose glue can
change later. The `searched` word is the forwarding pointer during collection.
The bookkeeping therefore uses side tables:

- Large clauses are keyed by proof id in an open-addressing `BinaryIdMap`.
  Proof ids survive arena collection. A cheap `[large_min, large_max]` id-range
  test runs first. `kernel_delete_clause` retires a tag exactly; an in-place
  shrink also counts, because it re-ids the clause.
- Binaries and units exist only in watch lists and on the trail. They keep
  stable external identities, plus internal filters: a per-literal mark byte and
  a binary-key table. `kernel_compact_variables` rebuilds the filters.
- `Kernel::symuse_active` is false until the first tag. It sits in existing
  padding after `warming`, and `SymmetryUse` is the last `Kernel` member, so no
  hot field moves.

**Hook placement.** A binary reason carries no clause pointer. Propagation is
therefore attributed after the fact. Each propagation entry point rescans the
trail segment it assigned and reads each literal's `(reason, binary, jumped)`.
A jumped binary is identified by `(lit, jumped_over)`. The entry points are
`propagate_service` (search), the probing entry that delegates to
`kernel_propagate`, probing recording, warm-up, binary-only propagation, and the
two conflict-reuse assertions. `kernel_assign`, `kernel_propagate` and
`propagate_literal` are unchanged. The bookkeeping functions live in their own
translation unit, `src/kernel/symmetry_use.cpp`. Analysis has one flag branch
per resolved antecedent.

## Counter definitions

`c symuse run R: N clauses (H helpers, U units), P propagations (Ph helper, Ps search), A analysis uses (Ah helper), Rf root forced, D deleted, L alive`

- **P**: assignments made by the entry points above whose reason is a tagged
  binary or large clause. **Ps** is the share made outside inprocessing, warm-up
  and probing. Lucky's direct `kernel_propagate` calls are not observed: on
  noL-11-6, v8 counted 303 more such propagations than v9.
- **A**: tagged clauses used by ConflictAnalysis, either as the conflict clause
  or as a resolved reason. This covers the 1UIP walk and the failed-level-one
  walk, but not minimization or shrinking.
- **Rf**: root implications whose reason contains the negation of a tagged unit.
  This is a one-level count: a unit's own consequences, not their descendants.
  When the run tagged no binary and no tagged large clause is live, only
  propagation at decision level 0 is inspected.
- **D** = N − L. **L** counts units, plus binaries still present in the watch
  lists at the end, plus large clauses never retired. D includes deletion by any
  path. Reduce is not separated, because `reduce.cpp` was outside this change's
  ownership.

## Validation

- Debug + ASan/UBSan, from committed HEAD `964f3569e` plus the patch: jobs
  1756608 (v2), 1756617 (v4), 1756694 (v5), 1756751 (v7), 1756758 (v8) and
  1756785 (v9). On v9 all of these pass: `test_symmetry_use`, `sr_proof_encoding`,
  `ulc_rat`, `analyze`, `kernel`, `clause_arena_replay`, `structured_handoff`
  and `online_inprocess`.
- `tests/test_symmetry_use.cpp` covers four cases:
  - A symmetric 3-clause formula publishes the SR cut (-1 2) under GraphSymmetry.
  - The cut is the propagation reason for 2 and is resolved in both the 1UIP
    walk (asserted == 1) and the failed-literal walk.
  - A tagged unit forces a root implication.
  - An ExplicitOnly cut is untagged. A tagged large clause retires on deletion.
    A tagged helper keeps its identity through variable compaction and is then
    counted as a helper propagation.
- Same-CPU sequential pairs (`make_pair.py`, `-c 8`, 32 GiB, c2 first) of c2
  (`build/pin-candidate-c2-rel/tactix`) against v9. Final jobs 1756787–1756795,
  in `benchmarks/runs/20261007-symuse-r5-*`.

| row | verdict | c2 s | v9 s | Δ wall | Δ instructions | Δ cycles | certificate | decisions/conflicts | other non-timing `c` lines |
|---|---|---|---|---|---|---|---|---|---|
| noL-11-6 | SAT | 39.9 | 41.3 | +3.4% | +0.7% | +3.4% | identical `0544e2c4dd45` | identical | identical |
| clqcl_30_11_10 | UNSAT | 79.2 | 83.7 | +5.7% | +0.4% | +5.6% | identical `1238a63812bb` | identical | identical |
| xor_op_n36_d3 | UNSAT | 85.7 | 73.7 | −14.0% | −0.0% | −14.1% | identical `c5fc62926f80` | identical | identical |
| stable-400 (no symmetry) | SAT | 97.5 | 101.2 | +3.7% | +0.0% | +3.7% | identical `11e4b76a0133` | identical | identical |
| knight_18 (no symmetry) | SAT | 147.0 | 139.7 | −5.0% | −0.1% | −5.0% | identical `16721acfe57f` | identical | identical |
| bphp_p23_h22 (v8, r4) | UNSAT | 537.1 | 579.1 | +7.8% | +0.1% | +7.8% | identical `a494b06049f6` | identical | identical |
| bphp_p23_h22 (v9, r5) | UNSAT | 536.7 | 575.4 | +7.2% | +0.1% | +7.2% | identical `a494b06049f6` | identical | identical |
| mp1-Nb7T42 | timeout 600 | — | — | — | — | — | partial | 138.0M/8.44M vs 132.5M/8.29M | n/a |
| noL-11-0 | timeout 1200 | — | — | — | — | — | partial | c2 printed none; v8 pair 93.8M/35.9M vs 90.5M/34.8M | n/a |
| snw_16_9_CCSpre | timeout 1200 | — | — | — | — | — | partial | 4.55M/1.40M vs 4.99M/1.51M | n/a |

Every row that finished has an identical certificate hash, identical decisions
and conflicts, and identical non-timing counter lines. Earlier revisions show
the same on all of their completed rows. On the three timeouts, the certificate
is a partial proof cut at the kill point, so trajectory identity cannot be read
from it. bphp's DSR check exceeds the 1800 s window in both arms (known).

## Cost

The bookkeeping's own time is small. On noL-11-6, the row with the heaviest
use, perf attributes 0.28 s to `kernel_symuse_note_trail` and 0.03 s to
`kernel_symuse_note_large_use`, together 0.75% of 41 s. On clqcl it is 0.35 s
(0.4%), and on bphp_p23_h22 it is 0.65 s of 591 s (0.1%).

The larger wall-time differences come from code placement, and their sign varies
by row:

- `kernel_propagate` and `propagate_literal` have identical normalized machine
  code in the two arms (objdump), and executed instruction counts match to
  0.03% on stable-400.
- Earlier changes elsewhere in the link moved `kernel_propagate` from 0x43cc10
  to 0x43d4c0, which changes its offset within 32 and 64 bytes.
- Perf puts the excess in `kernel_propagate`, whose code is identical:
  - stable-400: +5.1 s (50.4 s against 55.5 s), with no time in any
    bookkeeping function.
  - clqcl: +5.7 s.
  - bphp: +46.2 s (382.7 s against 428.9 s; job 1756835).
- Zen 4 front-end counters on stable-400 (job 1756811, AMD EPYC 9454) show the
  extra cycles come from instruction delivery:
  - `de_no_dispatch_per_slot.no_ops_from_frontend`: 518.2G vs 660.3G (+27%).
  - op-cache accesses: +10.8%, with fewer ops delivered from the op cache.
  - `ic_tag_hit_miss.instruction_cache_miss`: +17%.
  - Branch mispredictions: slightly lower.
- The same binary is 5.0% faster on knight_18 and 14% faster on xor_op. c2
  itself varied from 74 to 86 s on xor_op across pairs.

Iterations (each measured, then superseded; campaign dirs carry
`SUPERSEDED.md`):

1. **v1** (`20261007-symuse-*`): an out-of-line call in `kernel_assign`. Its
   failed-level-one analysis was uncounted.
2. **v4** (`r2`): an inline prefilter in `kernel_assign`. It executed 4.7% more
   instructions on stable-400 with the flag false, so the prefilter was removed.
3. **v5** (`r3`): an out-of-line call per assignment, which cost +5.5% on clqcl
   once active.
4. **v7/v8** (`r4`): a post-propagation trail rescan with a live-clause gate.
   Code growth in `kernel.cpp` changed GCC's inlining in `propagate_literal`,
   so the bookkeeping moved to its own translation unit.
5. **v9** (`r5`): the hook moved from `kernel_propagate` to
   `propagate_service`, which leaves the propagation code unchanged.

Per-function profiles are in `benchmarks/runs/20261007-symuse-perf/results-v8`,
`results-v9`, `results-v9-clqcl`, `results-v9-bphp` and `results-v9-frontend`. These runs used the
profiling exception: direct execution, with inputs, proofs and `perf.data` kept
in node scratch and deleted.

## Observed use (v9)

| row | symuse line |
|---|---|
| noL-11-6 | 1082 clauses (1079 helpers, 13 units), 2433023 propagations (2433021 helper, 2082578 search), 100226 analysis uses (100226 helper), 1 root forced, 721 deleted, 361 alive |
| clqcl_30_11_10 | 680 clauses (0 helpers, 271 units), 0 propagations, 0 analysis uses, 169 root forced, 409 deleted, 271 alive |
| xor_op_n36_d3 | 531 clauses (0 helpers, 531 units), 0 propagations, 0 analysis uses, 0 root forced, 0 deleted, 531 alive |
| bphp_p23_h22 | 233 clauses (0 helpers, 1 units), 4685637 propagations (0 helper, 4366055 search), 788 analysis uses, 0 root forced, 227 deleted, 6 alive (v8: 4685682 propagations; the difference is Lucky propagation, which v9 does not observe) |
| noL-11-0 | 1093 clauses (1089 helpers, 3 units), 2 propagations (0 helper, 0 search), 0 analysis uses, 1 root forced, 1090 deleted, 3 alive |
| mp1-Nb7T42 | 1 clauses (1 units), 0 propagations, 0 analysis uses, 0 root forced, 0 deleted, 1 alive |
| snw_16_9_CCSpre | 1 clauses (1 units), 0 propagations, 0 analysis uses, 0 root forced, 0 deleted, 1 alive |

The rows behave differently:

- On noL-11-0, almost all of the output is ULC helper binaries, and all but
  three tagged clauses are gone before search uses them. Search never reasons
  with them.
- On noL-11-6, the helpers carry 2.4M propagations and 100k analysis uses.
- On mp1 and snw, the output is one unit that forces nothing.
- On bphp, the 227 cuts propagate 4.7M times, but only 788 of those reasons
  reach a conflict.

## Limitations

- Each pair is one draw per arm, run sequentially on one CPU. Placement effects
  of ±5% are larger than the bookkeeping cost.
- A tagged binary stays tagged by literal pair. If it is deleted and an
  identical binary is re-added, the new binary is still counted.
- Reduce-specific deletion is not separated from other deletion.
- Lucky's direct propagation and Backjump's learned-clause assertions are not
  observed. Learned clauses are never tagged.
