# Approved experiment: full-width binary-tagged embedded watches

The user approved the isolated prototype after the explicit8-to12-byte watch
tradeoff and required exact behavior/proof plus runtime/memory validation were
presented. This is approval for the prototype, not adoption, promotion or any
FlatLists redesign. Current6730 remains the baseline;6822/6838/6857 are not
composed into this branch.

## Problem, design and invariants

Checked profile6817 onRank39 has7.44% self samples in embedded propagation.
Annotation6856 locates substantial watch traversal, clause loads and moves,
not a removable-cost ceiling. Pinned Kissat Kitten has an8-byte watch containing
a full literal and31-bit reference plus binary tag. Its binary path bypasses
the clause arena after the blocker test. Tactix has two full32-bit words and
still dereferences the arena even for a binary. CaDiCaL's embedded path lacks
this particular fast path; Kissat is the reference tie-break.

Prototype6869 keeps both32-bit payloads and adds a boolean, giving12bytes per
watch. It does not truncate references or lower public capacity. Attach records
whether size is2, watch copies retain the tag, large-watch moves set it false,
and core rebuilds derive tags anew. Only Kitten-mode propagation takes the
binary fast path. Fixed-mode clause orientation and model-flip behavior remain
unchanged. The original binary remains in the arena with its full proof ID.

The fast path must preserve stale-blocker semantics, unread conflict suffix,
root-unit materialization, learned clauses, assumptions, branch state and both
tick currencies. Its legacy entry meter still charges the two-literal scan.
Kitten's semantic allowance remains based on its reference8-byte watch, not
the prototype's larger host representation; name that source-derived currency
explicitly at propagation and model-flip use sites. No budget, cadence, option,
guard or public action change. Host RSS and runtime are measured separately.

## Migration, gates and rollback

1. Clone/hash-bind6730 separately. Add metadata, attachment/copy handling and
   the binary path as one complete internal change; never mutate a live pin.
2. Add fixed old/new tests for binary conflicts/units, blocking, root proof
   materialization, mixed clauses, replayed assumptions, both modes/meters,
   flips and core rebuilds. Require exact counters/models/derivation chains;
   run full suites and independent DRAT/LRAT checks.
3. Require all expected B28+C5 solver results and exact per-pair work and
   certificate bytes. Continue provisionally while proof checking runs; any
   invalidity pauses dependent work.
4. Run three matched waves on the existing32/39/38 controls. Price total
   runtime, Sweep/BVE components, peak RSS, placement and full arm spreads.
   Report every result; do not repeat to select favorable noise. Current-source
   fuzz and full50/memory acceptance remain separate prerequisites.

Expected benefit is lower binary propagation cost, not an assumed percentage.
Risk:50% more watch storage, extra copy traffic and cache pressure can outweigh
saved arena loads. A failed exactness premise or no useful runtime/memory
tradeoff rejects the candidate; do not tune representation or scheduling to
rescue it. Rollback is the unchanged8-byte6730 pin. New packing or reference
width changes require a separate review, not this approval.

This design follows the project's approved-revamp workflow. Its scope is a
faithful native binary-handling mechanism with a different full-width host
representation, not a new policy. Deadline remains Sep10 06:00EDT, with
winddown05:45; do not start tasks past the cutoff.
