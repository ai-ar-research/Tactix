# AM389: separate workspace checker drift from the H2 proof-state failure

Date: 2026-09-25. Checker-only diagnostic and fixed-micro replay; no solver,
proof-rule, canonical oracle, or checker-installation change. This follows
[AM388](2026-09-25-am388-sweep-core-allocation-boundary.md) and the
[AM369 H2 handoff](../runs/20260924-am369-condition-explicit-claude-fuzz/handoffs/H2-preexisting-dsr-sr-watch-pointer.md).

## Result

The corpus-034 `micro_proofs` failure reported in AM388 is **local checker
drift**, not an AM388 proof regression. The workspace
`tools/checkers/bin/dsr-trim` resolves to an untracked, modified
`dsr-trim-sr` build, SHA-256 `028240565e37185ce6064fe0f044d28ed70fb8b8823e03fdb2cc02988cbacc74`.
Its source contains additional uncommitted sparse forward-dependency logic
outside the repository-pinned patch in `tools/checkers/patches/`. The frozen
AM341/AM369 checker has SHA-256
`3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee`.
No local checker source, symlink, or active campaign pin was overwritten.

On the identical corpus-034 CNF and current proof (proof SHA-256
`aea599ca25f2f0e9cc248b023e87a6608301c7c6d371ad1759f0d5518c5f0570`),
both checker builds report forward DSR `s VERIFIED UNSAT`, but only the frozen
build emits an independently valid forward LSR. The workspace LSR's final
empty-clause line omits exactly four hints (`1348`, `349`, `296`, `1314`)
present in the frozen LSR; its other 6,431 text lines are identical. Pinned
Trestle reports `s INVALID` and CakeLPR reports failure at line 6431 for the
workspace LSR. The frozen LSR reports `s VERIFIED UNSAT` in **both** pinned
Trestle and CakeLPR. The frozen checker produces the same LSR with and without
`--delete-implied-units`, so this corpus-034 failure is distinct from H2.

The complete fixed-micro suite on the current AM388 Debug build passes when
its environment selects the frozen AM369 `pins/dsr-trim`, `pins/dsr-compress`
and `pins/srcheck`: `ctest --test-dir build/am388-sweep-stream-debug -R
'^micro_proofs$' --output-on-failure` gives **1/1 PASS in 37.54 s**. This
overrides checker executable paths only; the solver binary and test cases are
unchanged. The default local checker selection remains red and should not be
used to claim a solver failure. The dirty local checker clone belongs to its
owner; restoring or replacing it needs an explicit, non-destructive decision.

## Separate H2 gate

H2 remains a genuine canonical-route failure with the **frozen** checker.
No-option backward `dsr-trim` exits 1 at `Fatal error: [line 18] Clause 60 not
in watch pointers for literal 22`. The same frozen checker with
`-f --delete-implied-units` and pinned Trestle verifies this H2 DSR proof,
while an intentionally invalid witnessed-unit control is rejected in forward
mode at proof line 1. This is an option experiment, **not** approval to replace
the historical no-option acceptance route. AM387's bounded deletion-order
repair witnesses remain evidence, not a general emission algorithm. A user
decision on evaluating a checker-contract change is pending; preserve the
no-option route unless that decision authorizes a full positive/negative
acceptance audit.

## Provenance and next action

Inputs are the immutable H2 `triage/dsr-watch/{case.cnf,nox5.ascii.dsr}` and
AM388 `benchmarks/runs/20260925-am388-micro-substitute-diagnostic/{checker.cnf,current.dsr}`.
Frozen checkers are under `benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins/`;
Trestle SHA-256 is `19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945`
and CakeLPR SHA-256 is `ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57`.
The decoded LSR comparison and checker commands were run locally against
these fixed inputs; no real-instance solver run was launched. Use frozen
checker paths for current-source proof gates, preserve the dirty local clone,
obtain the H2 checker-contract decision or implement a general no-option
proof-state invariant, then rerun the combined proof/model gates. This audit
does not close N2, N1, N3, N4, or authorize Tier D.
