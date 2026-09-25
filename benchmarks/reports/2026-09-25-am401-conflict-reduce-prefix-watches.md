# AM401: preserve live sparse-prefix watches during conflict-state Reduce

Status: general N1 boundary repair, not combined N1/N2 acceptance. Source base
`e8a75b7f` (AM399/400). The targeted Debug `xdrv` binary before this commit
has SHA-256 `6547420e2dd330468776fbbb7fd6e71e4abcc5b39bfcb51462d8c3d1f14fb380`;
its source objects include this collector repair. All fixed and diagnostic
artifacts are under `benchmarks/runs/20260925-am401-h1-triage/`.

## Causal localization

The AM396 terminal Opus handoff H1 was a parent-identical Debug assertion,
`unit large clause escaped propagation`, on a public explicit action sequence.
The previous report gave a 47-variable CNF, seed 3965000531, and a 66-step
trace. Frozen Debug GDB jobs `1720799`, `1720809`, `1720825`, `1720831`,
`1720834` and `1720844` traced the live irredundant clause ID 211. Its
watch pair was `(79,4)`. Before Reduce at a recorded level-eight conflict,
the list for literal 4 had four words; immediately after Reduce it had zero,
while the clause remained live and its pair was unchanged. The next
ConflictAnalysis and Restart retained root assignments falsifying both 79
and 4. Full propagation scanned the root trail but could not see the clause;
HyperBinaryRes then exposed the missed unit at its public action boundary.
This is a lost-watch defect, not a reason to weaken the invariant or exclude
the HBR action.

The collector's sparse-prefix compaction stripped *all* large watches on a
root-fixed literal, normally safe because root collection had already
shortened/deleted affected clauses. Explicit Reduce is also legal while a
search conflict is recorded. That path deliberately defers root collection
to preserve the conflict antecedent, but reused the same root-watch strip.
AM401 keeps live prefix watches when `k.conflict_valid` is true, while still
dropping garbage entries, forwarding their Refs, and rewatching the sparse
tail. Default/root-simplified collection keeps its previous path. No new
guard, instance restriction, proof vocabulary or benchmark-specific dispatch
is introduced.

## Regression and independent checks

The new six-variable `restart_reduce` fixture holds two root-false literals,
a live irredundant prefix clause and a recorded search conflict while it
deletes one redundant tail victim. It requires both prefix watches to remain,
then restarts and requires the shared Propagate service to derive the pending
unit. It failed *before* the repair at `conflict-state Reduce dropped a live
prefix watch`; it now passes Debug, Release, and ASan+UBSan (leak detection
enabled). Focused Debug `restart_reduce`, `probe`, `lucky_terminal` and
`api_n1` pass 4/4. Documentation size/sync, rule-policy boundary and comment
hygiene checks pass; the six existing size-cap warnings remain.

SLURM array `1720867` replayed **both** exact AM396 H1 seeds through the
public-C `xdrv` on this Debug source, with no late additions: 3965000531
and 3966003082 each returned UNSAT without an invariant abort. Their native
DSR proofs (740 and 3,447 bytes respectively) independently verified against
the original CNFs through the AM369-pinned dsr-trim→LSR→Trestle route.
This exact small sample is not a fuzz acceptance or performance gate.

The combined AM399/400 Opus campaign and its full Debug CTest were launched
on their previous immutable pin and do not include AM401. A new immutable
AM401 affected fuzz campaign and full Debug suite are required. The separate
H2 no-option DSR rejection, post-finalization public addition contract,
broader N1/N2 audit and all N3/N4/performance gates remain open; no Tier D
submission is authorized by this repair.
