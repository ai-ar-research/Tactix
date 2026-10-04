# Affine recovery, partial-guidance telemetry, and scheduler-lifecycle diagnosis

## Disposition

**NOT ACCEPTED.** The focused campaigns recover all six unique instances that
the affine constructor solved directly, with independently checked original-CNF
models. They do not recover the three trajectory controls, and the partial-model
guidance source is never offered or consumed on this subset. The four regression
gates have not been rerun from the repaired scheduler lifecycle.

This report freezes the terminal evidence from four campaigns, the corrected
both-policy fuzz legs, the causal source/trajectory audit, and the subsequent
unit-tested repair. It does not rerun the saved `d170b4a5` comparison.

## Campaign identity and limits

| Arm | Source | Binary SHA-256 | Campaigns | Solver cap |
|---|---|---|---|---:|
| Affine control | `60446aa0d3e8e36c948577f0083922d35e2adeb6` | `262916d988bf98686005195ff8dac7d5c73fc46455c99d895c0b4c5b184b8820` | `20261004-affine-opportunity-recovery-1200`, `20261004-affine-opportunity-recovery-wv2-600` | 1,200 s; WV2 600 s |
| Guidance | `a49191bfa5e5b66ffbfb41750c3bd9bf71b50d18` | `7b959b074872d6245a0f8640535a4378bfcec6dfff86589f3aebdeb3f0a3bdc4` | `20261004-partial-guidance-recovery-1200`, `20261004-partial-guidance-recovery-wv2-600` | 1,200 s; WV2 600 s |

Both use a 1,800-second checker cap. SAT is accepted only after independent
checking against the original CNF. The scored records are
[`control-score.json`](2026-10-04-affine-guidance-recovery/control-score.json)
and [`guidance-score.json`](2026-10-04-affine-guidance-recovery/guidance-score.json).
Each score contains ten tasks, nine unique instances, seven direct-affine tasks
covering six unique instances, eight passing tasks, two failed timeout tasks,
and no pending task.

## Direct recovery and trajectory controls

| Instance | Accepted reference | Affine control | Guidance | Result |
|---|---:|---:|---:|---|
| `mdp-32-10` | — | 0.117 s | 0.117 s | SAT model checked; duplicate WV2 task also checked at 0.116/0.117 s |
| `mdp-28-14` | — | 0.067 s | 0.067 s | SAT model checked |
| `mdp-32-11` | — | 0.117 s | 0.117 s | SAT model checked |
| `mdp-32-14` | — | 0.317 s | 0.317 s | SAT model checked |
| `mdp-36-10` | — | 0.467 s | 0.467 s | SAT model checked |
| `mdp-36-14` | — | 0.467 s | 0.467 s | SAT model checked |
| `cabp-V-nos6` | 222.228 s | 1,127.160 s | 1,026.495 s | SAT model checked, but 5.07x/4.62x slower |
| `Break_20_72` | 134.586 s | timeout at 1,200.005 s | timeout at 1,200.006 s | unresolved |
| `Timetable_C_496...` | 482.234 s | timeout at 600.005 s | timeout at 600.005 s | unresolved |

Every direct MDP task reports exactly one affine call and one affine SAT
completion. The three controls report one affine call and no completion.
Therefore the omitted affine opportunity directly explains the six recovered
MDP losses, while it does not explain the three remaining search trajectories.

## Partial-model guidance observation

Every completed guidance row reports zero candidate offers, applications, Walk
imports/imported phases, CDCL target installs, eligible or changed phases,
target decisions/matches/overrides, replacements, and native/rephase/Walk
overwrites. Thus the guidance implementation has no measured effect on this
subset. The direct MDPs terminate inside affine construction; the three controls
publish no partial candidate. A later effect experiment needs instances that
actually produce a candidate and must separate availability, consumer use, and
checked result.

## Corrected policy fuzz evidence

The first jobs `1748409`/`1748410` exposed a reachability-parser mismatch with
the current telemetry syntax, not a solver failure. Commit `666f8bad0` corrected
the parser and added a positive/zero reachability regression. The replacement
jobs used harness SHA-256
`3d98ab22c0e76aa500d65de9de16a9077a2833b1a2a10782ad19f0039f2e4a7b`
and checker-set SHA-256
`895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8`.

| Job/policy | Corpus | Generated cases | Terminal oracle verdicts | Timeouts | Failures | `LocalSearchGuidance` reach |
|---|---:|---:|---:|---:|---:|---:|
| `1748417`, Default | 51/51 | 10,000 | 9,718 | 282 | 0 | 0 |
| `1748418`, random | 51/51 | 10,000 | 8,464 | 1,536 | 0 | 0 |

These are completed zero-found-failure legs, but the timeouts received no
model/proof/differential oracle and the new guidance action was not reached.
They establish compatibility and terminal-case correctness, not full 10,000-case
oracle coverage or effective guidance fuzz acceptance.

## Causal audit of the three regressions

The accepted `d170b4a5` approximation and latest saved good source `6fe14539`
have matching `cabp` and Break decisions, conflicts, BVE work, and structured
root output. Rejected `156afef6b` and recovered `60446aa0d` instead share the
same rejected structural prefix; restoring the affine call changes its counter
but does not restore that prefix.

| Row | Accepted structural prefix | Recovered `60446aa0d` prefix |
|---|---|---|
| `cabp` | SR 0; BVE 118,092 eliminated / 31,720,524 resolutions; 1,957,665 decisions / 86,308 conflicts | SR 1,350, all ULC binaries; BVE 845,002 / 198,496,847; 6,635,734 / 873,391 |
| Break | SR 0; BVE 45,987 / 8,682,428 | SR 31 applied with 31 ULC binaries; BVE 46,542 / 9,882,456; timeout |
| Timetable | SR 5 applied, zero ULC binaries; BVE 209,442 / 91,506,488 | SR 638 applied with 633 ULC binaries; BVE 210,408 / 101,726,430; timeout |

The earliest plausible change is `84e8d197a`, which removed Lucky/8 as stage 0
of the structured-root scheduler. `60446aa0d` invoked it directly before
`MaterializeColdState` and `run_structured_root`, outside the accepted
`InprocessScope` and stage transition. The structured-root rule code itself is
unchanged across the good-to-bad boundary. The exact-one counter also falls
from two to one, but the removed second call occurred after structured-root, so
it cannot cause this earlier divergence. This is strong causal inference from
saved endpoints; no run at `84e8d197a` proves a first-bad commit.

Commit `f6d7453f6e9f1ae55c34b333ae999b26ac4514ac` restores Lucky/8 as typed
structured-root step 109, removes the direct call, synchronizes its configured
parameters and disable switch, and retains the public action as the only
implementation. A feedback-level test pins stage order, no-op status, formula
version stability, the strategy-epoch transition, and subsequent structural
execution. The combined checkpoint suite passes 10/10 in both the normal and
ASan/UBSan builds. No benchmark result in this report was produced from
`f6d7453f6`.

## Concurrent certified-UNSAT foundations

The current tree also contains three internal, unaccepted clausal verticals:

- GF(3) has an independently replayed Gaussian-to-RUP certificate for unit and
  conflict outcomes, including bounded compact source references; it has no
  live publisher and does not support nonunit rows or auxiliary/PB steps.
- Direct PHP has exact live terminal DSR/DRAT publication and a Kernel-bound
  unregistered action/replay session. Proof-backed split prepare/publish is
  deliberately rejected because `Proof` exposes no exact mutation epoch.
- Tseitin-cycle X7 has exact live replay and DSR/no-proof publication for one
  caller-authenticated cycle; it has no autonomous group discovery or public
  action.

Their normal and ASan/UBSan focused tests pass. They make no target-recovery or
public-controller claim.

## Next acceptance step

When another benchmark run is authorized, test only `cabp`, Break, and
Timetable from `f6d7453f6` first. Require the accepted structural prefix before
using wall time as the decision signal: `cabp` and Break must produce zero ULC
binaries, and Timetable must produce five SR applications with zero ULC
binaries. If that prefix returns, proceed to the owner subset and then the
B28+C5, Rank1--50, WV100, and WV2--89 gates. Keep the 1,800-second bounded proof
check and the documented checker-slowdown rule. Full Debug, effective
both-policy fuzz, all 44 targets, and N1--N4 verification remain open.
