# Sweep ledger membership: avoid repeated retained-prefix scans

## Outcome and scope

Candidate **5473** replaces two repeated full-range membership searches in
`SweepSubstitutionSession::rewrite_large_in_place` with private wraparound
cursors. Both independent Release suites pass **84/84**, and **588** fixed
bookkeeping transitions have the same digest with actual predecessor and new
objects. Three exact-work timing pairs and the unchanged B28+C5 gate are
submitted. No new speedup is established; the candidate remains isolated and
unfuzzed on top of watch-pair 5466.

The Rank 1–40 objective remains at most 1.01x Kissat. Latest complete solver
view 5454 is 1.170282883x, with checking pending; production remains unchanged
at checked 1.2787518x. Newer full bound campaign 5465 is running and does not yet
have all 80 solver records. Partial results are not scored as the full target.

## Evidence selects the change

Clean profile 5425 assigns 1.02% self samples to this Sweep bookkeeping helper.
[Annotation 5472](../runs/20260908-5472-sweep-rewrite-annotation/manifest.json)
validates the original compressed/raw profile and binary identities, then
attributes the existing 727 local samples. Nearly all local periods fall in
the two membership-search loops, not the occurrence removal or append calls.
Samples identify a cost to investigate; they are not a measured removable
runtime fraction or an acceptance timing claim.

Kissat `sweep.c::substitute_connected_clauses` and CaDiCaL
`sweep.cpp::substitute_connected_clauses` construct surviving images in source
order. Tactix's ledger checks each old literal against the image and each image
literal against the source. Restarting each successful search at the beginning
makes unchanged runs quadratic. The adjacent kernel replacement optimization
already uses exact wraparound membership; this applies the same approach to
the separate Sweep ledger without changing its ownership or traversal order.

For each query the cursor searches `[next,end)` then `[begin,next)`, advancing
only after a match. The boolean is therefore exactly full-range membership,
including arbitrary reordering and misses. Both literal ranges remain unchanged
throughout the helper. No marks, shared scratch, allocation, update ordering,
clause pose, rule effect/guard, public instance surface, proof/reconstruction,
scheduling or effort changes. Adversarial misses/reordering may remain
quadratic; this is not a worst-case linear algorithm or a structural rewrite.
Existing V3/V5 calculus and implementation contracts remain unchanged.

## Unit-test consideration and implementation authority

The coverage review found no direct fixture spanning this helper's membership
shapes and delayed-move path. New `test_sweep_cursor.cpp` covers seven lengths
(3–129), seven images (identity, removal, replacement, all-new, rotation,
reversal, mixed), both clause kinds, sparse/dense/ordered-dense modes, and direct
or transaction-delayed moves: **588 cases**. It asserts exact image occurrence
membership, no formula-arena mutation, and hashes ledger ordering, capacity,
initialization and dirty-state after flushing moves. Both actual old and new
objects produce **`b15f97a272f9abef`**. Existing substitute/Sweep API and proof
tests remain part of the complete suites. No solver was run on a real instance
outside SLURM; these are fixed bookkeeping tests, not a fuzz campaign.

[5473 manifest](../runs/20260908-5473-sweep-cursor/manifest.json) owns the source,
parent, exact patches and logs. Release 5473 and independent BC 5474 pass 84/84
in **5.65/6.95 s** and share binary SHA-256
`305b2285fdda58345bdb020efe41584b3e8a4025df8aa8f4c44a06fa18f3f594`.
It descends from `.claude/worktrees/runtime-watch-pair-5466`, excludes independent
metadata 5426, and is frozen at `.claude/worktrees/runtime-sweep-cursor-5473`.

## Gates and continuation

- **5475 / job 1671162:** three paired Rank-35 waves against exact predecessor
  5466. Require complete non-timing statistics and proof-byte identity in every
  pair before judging median runtime ratio and full range. Absolute timing
  needs both arm ranges below 2%; pending proofs never become checked evidence.
- **5476 / job 1671163:** unchanged cumulative B28+C5 candidate/production gate.
  Two independent candidate builds have matching hashes. No instance selection,
  runner, proof/check contract or acceptance threshold changed.

Both campaigns are full width, one proof-on solver per 12-logical-CPU/48000-MiB
task, no throttle or node constraint, with the existing atomic solve/check/
cleanup runner. Checkers remain background work during development.

[5448 final gate](../runs/20260907-5448-replacement-cursor-BC-gate/summary.json)
is now **66/66 checked and clean**, no timeout or solve loss: B 0.809092079x,
C 0.983162126x, combined 0.919288592x production. It validates that predecessor,
not the newer candidate or the Rank 1–40 objective.

Rank-31 and Square causal probes remain queued. Rank-37's 100000-conflict
closure remains established by 5463, not a complete full-run timing result.
The new full-bound snapshot helper will score 5465 only when every pair exists.
Claude fuzz owner session 30094 is still live and covers only 5438; no fuzz
resource was inspected. Its next periodic checkpoint must include all changes
through 5473. Actual soundness/crash/proof/model failures pause dependent work.

Production fixed tests pass 77/77 in 5.08 s. The wrap-up audit and Andrew report
own final documentation, pin and residue checks. The annotation helper removed
only its self-created temporary decompressed profile; retained compressed/raw
evidence identities remain available. No manual campaign data deletion,
promotion, push retry or separate-checker contract change occurred. Push is
still unapproved. Next evaluate the pending causal traces and complete runtime
views; neutral/noisy cursor timing does not establish a useful optimization.
