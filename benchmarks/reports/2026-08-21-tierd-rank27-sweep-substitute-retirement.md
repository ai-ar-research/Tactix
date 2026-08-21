# Tier-D rank 27: Sweep-to-Substitute retirement-order repair

## Outcome

The post-Vivify rank-27 divergence was not a reason to restore the removed
variable-range policy.  The retained traces instead localized the first
post-Probe search split to an equal-score EVSIDS heap pose.  Tactix and Kissat
held identical scores and retired the same 369 variables, but their heap
removal order first differed at ordinal 27.  The resulting heap roots selected
different decision 35 literals.

Kissat's source explains the ordering contract.  Sweep rewrites connected
clauses and fills `sweeper->reprs`, but does not deactivate the proved
representatives.  The one Substitute call immediately after Sweep discovers
the complete SCC union and deactivates it with `for (all_variables (idx))`.
Those compact internal indices are Tactix's permanent first-import order.
Tactix must create reconstruction witnesses per environment because its
variable-width representation is rewritten inline, but it may not expose
those environment batches as heuristic retirement order.

The general repair makes ordinary Substitute emit witnesses/retirement events
in first-import order, normalizes the complete Sweep-produced suffix once per
Sweep call, and extends the same suffix across DefaultPolicy's mandated
post-Sweep Substitute.  Vivify's earlier retirement prefix remains ahead of
the Sweep/Substitute union.  No benchmark identity, rank, hash, literal,
variable range, deadline, or tuned constant enters solver behavior.

Release passes 72/72.  Candidate-only job `1637838` matches all 369 retained
Kissat heap-removal payloads exactly and matches all 481 conflict-3069
decisions through conflict 3070.  Matched 28B+5C job `1637839` passes 66/66:
all 34 SAT models and 32 UNSAT proofs verify, there is no solve loss, and
certificate residue is zero.  Immutable rank-27 witness job `1637906` is
recorded below after its required 16 GiB/1200 s run.

## Frozen identities and discipline

- Immutable expected-UNSAT input:
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`.
- Compressed SHA-256:
  `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
- Accepted binary SHA-256:
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`.
- Candidate binary SHA-256:
  `50feae80dd275e8178d2d14bd975517b35cdf7996bf2f6461db0fccdbe20a925`,
  base `729f08537c78dc08242f65a9d5cac0bb88c916d7`, source-diff SHA-256
  `c2ef67762acb0a8f56d1245b25e42b3f99e07f72059da23f4f9a83d61ebb1954`.
- Retained Kissat heap-removal trace SHA-256:
  `c3347f91ffc2950902161ee9231df5364461bba2d8559751230810f669d2a748`.

The branch and `origin/main` began synchronized.  Surviving worktrees and the
unrelated SLURM shell job were inspected and left untouched.  No frozen
reference solver was rerun.  Candidate localization used 16 GiB/300 s; the
guard and witness used the pre-Phase-4.5 16 GiB/1200 s contract.

## Common-state localization

The retained conflict-3069 trace first ruled out score drift.  After the 34th
shared post-Probe decision, Kissat's heap root was external variable 230665
and Tactix's was 230802, both with score
`2.6056478753115483e+24`.  Both traces contained exactly 369 retirement
removals and the removed-variable sets were identical.  Payloads matched for
the first 26 operations; removal 27 was 95250 in Kissat and 95251 in Tactix.

The first attempted repair ordered each environment by its equivalence
emission sequence.  Job `1637835` rejected it: Kissat began the relevant
suffix `95250, 95251, 95252, ...`, while that candidate began `95251, 420557,
...`.  Source inspection then showed that pair-emission order controls moved
watch streams, not variable deactivation.

Ordering each environment by import rank was also insufficient because
Tactix has many inline environments while Kissat has one later Substitute
scan.  Job `1637837` matched removals 1--363, then placed four Sweep variables
before two SCC variables discovered by the follow-up Substitute.  The final
repair carries the Sweep suffix boundary across that follow-up and sorts the
complete union.  Job `1637838` produces the same payload digest as Kissat:
`ad1a51890cf7fff2ac62f46b9ddbf295f690fd9eccc4ea2dd06b00757834f1a7`.
The payload includes removed variable, exact score, old heap position, swapped
tail variable, source size, and final position, so this is full heap-mutation
parity rather than set equality.

The repaired candidate also matches the retained Kissat decision sequence for
all 481 decisions at conflict 3069, digest
`fd9a98841c5f5d7b38d2cc4ee10233a00fcb2bf152f37e17894e05548c2b6f98`.
The former decision-35 split is therefore closed.

## Small general repair and tests

1. Substitute walks reconstruction witnesses and retirement events in
   `kernel_vars_in_import_order`, matching Kissat's compact `all_variables`
   scan rather than Tactix's stable external-backed `Var` numbering.
2. A Sweep call sorts only the retirement/substitution suffix it produced;
   earlier Vivify and Substitute events keep their stage order.
3. DefaultPolicy carries that suffix start through the immediately following
   Substitute, matching the one union Kissat deactivates after Sweep.

The Substitute regression builds a three-variable SCC whose numeric and
first-import retirement orders disagree.  The Sweep regression forces two
productive environments in reverse import order and requires one collection
plus an import-ordered retirement suffix.  The pre-repair ordering assertion
fails.  No unit test or fuzzing exposed a soundness defect or crash, so the
plan's Claude repair-delegation trigger did not fire.

## Full B+C guard

Campaign `benchmarks/runs/20260821-1215-BC-sweep-substitute-retirement/`, job
`1637839`, ran all 28 Tier-B fixtures and the inherited fixed five
mechanism-relevant Tier-C fixtures.

| arm | valid | SAT / UNSAT | aggregate / PAR-2 | peak RSS |
|---|---:|---:|---:|---:|
| accepted pin | 33/33 | 17 / 16 | 2212.751 s | 3.233 GiB |
| candidate | 33/33 | 17 / 16 | 3071.347 s | 2.969 GiB |

The ratio is 1.388022.  There are no solve losses, missing or malformed
results, checker failures, cleanup failures, or residual certificates.
Candidate `T105.2.0` proves UNSAT in 863.983 s versus 794.595 s for the
accepted pin; both proofs verify and are deleted.  Candidate `mp1-Nb5T15`
remains checked SAT in 716.185 s.

## Immutable rank-27 witness

Only after the full guard passed was campaign
`benchmarks/runs/20260821-1300-tierd-rank27-sweep-substitute-retirement/`
submitted as job `1637906`.  It returned UNKNOWN at 1200.004 s.  The runner
recorded a 132,120,576-byte partial proof, SHA-256
`7f193dcbba55084ab9a4f5641506f479d3b71fc7c66809affbf7067e3ab73fc6`,
then deleted it and all derived scratch.  Certificate residue is zero.  Rank
27 is not added to the nine closed ranks.

## Next action

Retain the closed Vivify scope/deduction/interleaving, TransitiveReduce order,
Sweep semantic dose/lifecycle/schedule, and this exact Sweep-to-Substitute
retirement transaction.  If rank 27 remains open, the next common-state
localization begins after the exact conflict-3069 heap mutations and 481
decisions, not at Vivify or the removed variable-range policy.
