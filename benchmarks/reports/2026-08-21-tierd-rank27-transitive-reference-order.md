# Tier-D rank 27: post-Vivify TransitiveReduce reference order

## Outcome

The rank-27 Vivify repair remains closed at candidate scope, cumulative dose,
deduction, terminal effect, and productivity. The first portable divergence
after those matched calls was the TransitiveReduce pass between them. Tactix
spent essentially the same tick ceiling in ascending literal order and deleted
one binary over the bounded trajectory; Kissat activity-sorted its sources and
deleted 85 in the retained run, including 21 at the conflict-3069 boundary.

The exact source detail is mechanically unusual but general: `transitive.c`
pushes variable indices, applies `IDX` once more inside its EVSIDS/VMTF
comparator, sorts increasing, and pops from the back. Tactix now poses the
equivalent V4-only front-consumed order. No benchmark name, hash, rank,
literal, deadline, or tuned constant enters production behavior. The generic
heuristic candidate-order meaning is unchanged for other rules.

Release passes 72/72. Matched 28B+5C job `1637542` is 66/66 valid: all 34 SAT
models and 32 UNSAT proofs verify, with no malformed, missing, cleanup-bad, or
residual-certificate task. Immutable rank-27 witness `1637608` remains UNKNOWN
at 1200.004 seconds. Its 147,849,216-byte partial proof was hashed and deleted,
so rank 27 remains open.

## Frozen identities and discipline

- Immutable expected-UNSAT input:
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`.
- Compressed SHA-256:
  `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
- Accepted binary SHA-256:
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`,
  source `7b527fbc711d664898af8e9c5a9655c3bdf4204b`.
- Candidate binary SHA-256:
  `4caa9276df3b03b685a168332ae2335ab2ce0d6c5cbc2f88f61c7eba934702a0`,
  base `62f3bca81ed9d9b40099e606689f8420ddcb0b97`, source-diff SHA-256
  `110eafdfa19d88a2ade10fff7a1b3f43ca72d89a1f8cf3ac3a574a5acc2dac85`.

The branch and `origin/main` began synchronized. Surviving worktrees and
unrelated SLURM jobs were inspected and left untouched. The frozen rank-21--40
manifest, six inherited closures, and prior reports were read first. No frozen
reference solver was rerun; localization used retained Kissat traces and
Tactix-only source-pinned arms. All real solver work used SLURM. Diagnostics
used 16 GiB/300 s; the guard and witness used 16 GiB/1200 s.

## Localization

At conflict 3069, repaired Tactix reaches the same next Probe trigger and the
same two Vivify long-clause populations and aggregate terminal effects as
Kissat. The first later decision divergence was traced backward from decision
35 to heap topology: both solvers had the same scores, but Probe removed 369
Kissat variables versus 33 Tactix variables. Slot snapshots placed the first
formula owner at TransitiveReduce between the Vivify calls.

Source comparison then exposed three candidate mechanics:

1. Kissat orders active variables by mode-dependent activity; Tactix enumerated
   ascending source literals.
2. Kissat stops a BFS as soon as its destination is reached; Tactix normally
   completes the walk to retain failed-literal detection.
3. Kissat's comparator applies `IDX` to an item that is already a variable
   index. Thus its effective activity proxy is `idx >> 1`, not the scheduled
   variable itself. Its increasing sort is consumed from the back.

Job `1637470` tested native order, early exit alone, ordinary own-variable
heuristic order, and their combination. Early exit was bit-identical; the
ordinary heuristic changed the work slice but deleted zero binaries at the
boundary. That candidate was reverted.

Job `1637474` posed the literal reference comparator, with and without early
exit. Both arms were identical. The first TransitiveReduce call changed from
zero deletions to 24; the retained Kissat trace deletes 21. Binary populations
at the first call end are 751,109 Tactix versus 751,102 Kissat, reducing the
pre-call ten-binary excess to seven. After the second Vivify, Tactix has
751,122 binaries versus Kissat's 751,112. Later Sweep/Substitute behavior
brings the final counts close but through different equivalence/retained-root
representations. That is the next open boundary; it is not evidence to reopen
Vivify deduction or TransitiveReduce source order.

## Implementation and tests

Production changes are confined to policy scheduling:

- `DefaultCandidateOrder` recognizes only the TransitiveReduce heuristic arm
  as the literal reference comparator (`v >> 1` proxy, decreasing activity,
  decreasing-variable tie, stable sign order);
- the default TransitiveReduce slot selects that arm;
- all other candidate-order arms and rules keep their previous meaning; and
- early exit remains unchanged because the one-at-a-time diagnostic was inert.

The focused scheduling test independently computes the reference proxy and
requires its best candidate first. The slot-membership test requires V4 to ship
with the heuristic arm. Release passes all 72 tests, including the
TransitiveReduce brute-force equivalence oracle and proof tests. No fuzzing or
unit test exposed a crash or soundness defect, so the plan's mandatory Claude
repair-delegation trigger did not fire.

## Full B+C guard

Campaign `benchmarks/runs/20260821-1830-BC-transitive-reference-order/` ran the
accepted pin and candidate on all 28 Tier-B fixtures plus the inherited fixed
five Tier-C fixtures. The first submission (`1637476`) was invalid harness
setup: `matrix.tsv` had been omitted, so every task failed before input
decompression or solver invocation. It produced no result or certificate
residue. The unchanged frozen matrix was restored and job `1637542` completed.

| arm | valid | SAT / UNSAT | aggregate | peak RSS |
|---|---:|---:|---:|---:|
| accepted pin | 33/33 | 17 / 16 | 2202.399 s | 3.234 GiB |
| candidate | 33/33 | 17 / 16 | 2773.863 s | 3.360 GiB |

All 66 tasks are valid. The candidate ratio is 1.259473. This keeps the
previously reopened `mp1-Nb5T15`/`sted5` performance coupling active; the
general source repair is not promoted as a new accepted performance pin.

## Rank-27 witness and next action

Only after the full guard passed was job `1637608` submitted. It returned
UNKNOWN at 1200.004 s. The runner recorded a 147,849,216-byte partial proof,
SHA-256
`38b4c082e88e95f0bd11a1486d9ac2fdc46671b8b806a6796f8b5e9c7df85033`,
then deleted it and all derived scratch. There is zero certificate residue.
The prior Vivify-only witness produced a 252,706,816-byte partial proof at the
same deadline; the smaller artifact is descriptive trajectory evidence, not a
performance conclusion or closure.

Rank 27 remains open. The next UNSAT-first localization starts at the residual
post-Vivify Sweep/equivalence and retained-root representation split. It must
not repeat the completed Vivify candidate/deduction work or the now-closed
TransitiveReduce source-order attribution. Any further source repair again
owes the full 28B+fixed-C guard before a Tier-D witness.
