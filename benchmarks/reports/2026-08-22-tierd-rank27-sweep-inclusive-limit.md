# Tier-D rank 27: inclusive Kitten limit repair

## Outcome

The three residual Sweep seed visits were one scheduling defect, not three
eligibility exceptions. Tactix's Kitten-mode sub-solver allowed work while its
counter was exactly the environment limit and stopped only after an overrun.
Kissat stops embedded search at `ticks >= limit`, but its enclosing Sweep call
starts another seed until cumulative ticks are strictly greater than the round
limit. That one comparator difference placed seed 9080 in different Sweep
sessions, advanced the deterministic Kitten generator differently, and caused
all later local equivalence and representative-order differences.

Tactix now uses the reference's inclusive poll for Kitten-mode decisions and
the outer backbone/partition refinement boundaries. The legacy entry-counter
budget used by BVE retains its existing strict-overrun meaning, and the
enclosing Sweep call still uses Kissat's strict `>` cutoff.

Candidate-only job `1637761` matches the retained Kissat artifact exactly
through the localized boundary: 595/595 opening seeds, 2,303/2,303 resumed
seeds, and all 343 equivalence identities in the same discovery order. Release
passes 72/72. Matched 28B+5C job `1637762` is 66/66 valid with zero certificate
residue and improves the candidate aggregate from 2924.991 s to 2868.252 s.
Immutable rank-27 witness `1637828` remains UNKNOWN at 1200.004 s; its partial
proof was hashed and deleted. Rank 27 therefore remains open.

## Frozen identities and discipline

- Immutable expected-UNSAT input:
  `tierD24-26/5911653e4edadec391654c8f06353f9a-oski15a01b06s_opt.cnf.xz`.
- Compressed SHA-256:
  `5435b1b271b049ed9291ab1b9b297afb33b8d2d3c50ff1b48bfaed4ddf7bb4e9`.
- Accepted binary SHA-256:
  `b6c991e099f4fa9cc635bacb20d7aa44ff3fdc23d0af78974b12998cecd6fb6f`,
  source `7b527fbc711d664898af8e9c5a9655c3bdf4204b`.
- Candidate binary SHA-256:
  `09a771595e833451111d495745a245f7568130de5163b51cbac295c721817ae3`,
  base `741870c66c3ec6fb9bd6d584bcb004b142633b13`, source-diff SHA-256
  `ba146a9bb01dd5097d9c15b58abe1475c50cc13da11576e0801787fdefa18eba`.
- Retained Kissat trace SHA-256:
  `a6ffd4e0d4fa27e5c07c45a1b53a3f1e049bad3bd869b49dd929a9adf842e59c`.

The branch and `origin/main` began synchronized. The surviving unrelated SLURM
shell and worktrees were inspected and left untouched. Frozen reports,
manifests, the rank-27 identity, and the accepted pin were read before changes.
No frozen reference solver was rerun. Candidate-only localization used
16 GiB/300 s; the full guard and witness used the pre-Phase-4.5
16 GiB/1200 s contract.

## Localization from the retained schedule

The first diagnostic campaign,
`benchmarks/runs/20260822-0030-tierd-rank27-sweep-retained-tail/`, job
`1637760`, traced both relevant Sweep sessions from production source plus
trace-only event admission.

At conflict 0, Tactix and retained Kissat visit the same first 594 seeds in the
same order. Kissat then visits seed 9080 as seed 595; Tactix stops and retains
9080. At conflict 3069, Tactix consequently begins with 9080 and visits 2,306
seeds, while Kissat begins with 9132 and visits 2,303. The apparent three-seed
difference is therefore one seed shifted across the session boundary plus two
later tail positions made reachable by the changed per-session trajectory.

Tactix's first call ended after seed 9001 at exactly 1,000,001 semantic ticks.
The source comparison explains the boundary:

1. Kissat's embedded `decide` returns UNKNOWN when
   `KITTEN_TICKS >= kitten->limits.ticks` (`kitten.c:1572`).
2. Sweep's outer refinement helper also polls inclusively through
   `kitten_ticks_limit_hit` (`sweep.c:53-68,1369-1420`).
3. The enclosing seed loop stops only when cumulative ticks are strictly
   greater than its round limit (`sweep.c:1667-1683`).
4. Tactix used `spent > budget` inside `SubSolver::solve`, so it allowed one
   additional decision at equality and crossed to 1,000,001. Its enclosing
   strict-overrun check then correctly declined seed 9080.

The seed's session is semantic because `start_kitten_session` resets the
deterministic generator while each environment clear advances it. Moving 9080
to the next session shifted every later model phase. The previously observed
four-way representative reorders were downstream of that generator shift, not
a defect in `schedule_inner` emulation.

## Small general repair

The production change is confined to budget predicates:

1. `SubSolver::solve` uses `>=` only when `SubTickMode::Kitten` is selected.
   Its older entry meter keeps `>` for BVE and other non-Kitten callers.
2. Sweep checks `>=` at the top of each backbone and equivalence refinement
   iteration and after each whole flip pass, matching
   `kitten_ticks_limit_hit`.
3. The inner flip loops retain their source-backed `>` checks, and the enclosing
   Sweep seed loop retains its source-backed `>` cutoff.

No option, formula-size arm, benchmark identity, rank, hash, literal, deadline,
or new constant enters solver behavior. A focused regression imports three
variables with one ternary clause and verifies that Kitten mode returns Budget
when its tick counter equals the limit; the prior strict comparator returned
SAT after one extra decision.

No unit test or fuzzing exposed a soundness defect or crash, so the plan's
mandatory Claude repair-delegation trigger did not fire.

## Exact diagnostic result

Campaign
`benchmarks/runs/20260822-0115-tierd-rank27-sweep-inclusive-limit/`, job
`1637761`, used the repaired trace pin and stopped after conflict 3069.

| boundary | Tactix | retained Kissat | result |
|---|---:|---:|---|
| conflict-0 seeds | 595 | 595 | exact identity/order |
| conflict-3069 seeds | 2,303 | 2,303 | exact identity/order |
| conflict-3069 equivalences | 343 | 343 | exact normalized identity/order |
| opening final seed | 9080 | 9080 | same session |
| resumed final seed | 347737 | 347737 | same ceiling tail |

The repaired first session spends 1,000,131 ticks because the enclosing Sweep
loop is allowed to begin seed 9080 at equality; that seed spends 131 ticks and
then the strict outer cutoff stops the call. This is the layered reference
contract, not an unexplained overrun. The second call spends 19,189,801 ticks
and retains the exact Kissat seed/equivalence sequence.

## Verification

Release CTest is 72/72, including the focused SubSolver boundary test, Sweep
proof replay, randomized/brute-force sub-solver coverage, and documentation
conformance.

Matched campaign
`benchmarks/runs/20260822-0200-BC-sweep-inclusive-limit/`, job `1637762`, ran
the accepted pin and candidate over every Tier-B fixture plus the inherited
fixed five mechanism-relevant Tier-C fixtures.

| arm | valid | SAT / UNSAT | aggregate / PAR-2 | peak RSS |
|---|---:|---:|---:|---:|
| accepted pin | 33/33 | 17 / 16 | 2202.153 s | 3.234 GiB |
| candidate | 33/33 | 17 / 16 | 2868.252 s | 3.036 GiB |

The ratio is 1.302476. All 34 SAT models and 32 UNSAT proofs verify. There are
no invalid, missing, malformed, cleanup-bad, or residual-certificate tasks.
Candidate `T105.2.0` proves UNSAT in 1007.801 s with a checked and deleted
1,267,849,928-byte proof, improving the prior lifecycle candidate's 1043.578 s.
Candidate `mp1-Nb5T15` remains checked SAT at 713.803 s.

## Rank-27 witness and next action

Only after the full guard passed was campaign
`benchmarks/runs/20260822-0245-tierd-rank27-sweep-inclusive-limit/`, job
`1637828`, submitted. It returned UNKNOWN at 1200.004 s. The runner recorded a
303,038,464-byte partial proof with SHA-256
`0d49d51144cc8a057bab4de5e9fe568810e1820b0f8b8b58670f5673ff7426d1`,
then deleted it and all derived scratch. Certificate residue is zero.

Rank 27 is not added to the nine closed ranks. The next localization retains
the closed Vivify scope/deduction/interleaving repairs, TransitiveReduce order,
Sweep semantic/representation lifecycle, and now the exact first two Sweep
seed and equivalence schedules. It begins at the earliest later common-state
search or inprocessing divergence; the shifted-seed and representative-order
investigations must not be repeated. Any further source repair again owes the
complete 28B+fixed-C guard before another Tier-D witness.
