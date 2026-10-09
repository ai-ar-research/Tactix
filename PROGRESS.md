# Progress and resume guide

## Current stage

Session goal (user): at least 10 independently verified fixed44 closures on one coherent
candidate, preserving N1–N4, with effective Claude-owned fuzz, full Debug and all four
regression gates ([DEVELOPMENT_PLAN.md §9.5](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration)).
**Met by c2.1.**

- **Qualified candidate: c2.1 = `1aad09b07`** (branch `candidate-c2.1`; c2 + test fix +
  affine refresh root-unit fix; search byte-identical to c2). Release `28995e13…`.
  - full Debug 353/353;
  - fuzz PASS, 0 product failures;
  - B28+C5 accepted;
  - Rank1–50, WV100 and WV2–89 pass under §8.4 (5,000 s retention for noL-11-0,
    noL-11-8 and Circuit_multiplier24; accepted exception snw_16_9_CCSpre);
  - **13 verified fixed44 closures**: ranks 147, 226, 263, 489, 497, 511, 541, 627, 630, 899,
    991, 1075, 1129.
  - [Qualification report](benchmarks/reports/2026-10-08-candidate-c2.1-qualification/README.md).
- **c3/c4 (main lineage, not qualified).** Satsuma A–D, one-round pure literals, limit
  repair, per-pass refinement (`2b1544cbb`), fix_and_cut scan (`e0e09d678`, held) and
  witness-free SR expansion (`163429b44`). They fail B28+C5 and lose Break_20_72 and
  ntil-90d-40.
- **Trajectory finding** (`benchmarks/probes/scratch/traj-advgc/`):
  - On Satsuma's adv_gc residual (input + 19,800 AMO binaries), Tactix needs 155M conflicts
    against Kissat fastel=1's 21M. Preprocessing is identical, so this is a Kissat-port
    search gap.
  - Symmetry-off c2 solves noL-11-10 (110 s), adv_gc (170 s), SDP_118 (1,235 s),
    ITC2021_Early_10 (1,348 s) and Timetable_E48 (1,522 s), all of which c2 misses.
  - mp1-Nb5T15's gap is startup FastEliminate (Kissat 4.0.4 fastel=0); battleship's is
    clause-order chaos.
  - snw16's unit witness is 70,132/70,855 sign flips; the phase test (job 1758755) is
    pending.
- **Affine search**: the refresh fix is in c2.1. Admission + Gauss-Jordan are in progress,
  then it becomes default-on.

## Next action

1. Next candidate (c5), gated by counter identity on the gate rows first, then the four gates:
   the search-divergence fix (trace agent), the phase transform for symmetry units if the
   snw16 test confirms it, `163429b44`, affine-on. The target is more fixed44 solves with
   no gate loss.
2. Resume the symmetry-off screen of the remaining 27 gap rows (cancelled for gate
   capacity; rows 401, 654, 781 and 887 are done).
3. Baseline reruns (A)/(B) are running under the governor; (C) follows (A). Then the full
   Tactix Tier D run on c2.1 (cpu-q, -c 4, 32 GB, 5,000 s, 8 h check, 125 cap, governor).
4. Open: Rank291 needs a checker-side route (DRAT expansion exceeds 32 GB). Constant-image
   witness expansion is deferred.

## Live work

| Work | Jobs / owner | Status |
|---|---|---|
| Search divergence (adv_gc residual) | trace agent | running |
| Affine admission + Gauss-Jordan | repair agent | running |
| snw16 phase test / Kissat model | 1758755, 1758743 (`benchmarks/probes/scratch/snw16-unit/`) | running |
| Rank569 witness-free full check | 1758604 (`benchmarks/runs/20261008-srexp-r569-fullcheck`) | running |
| c4 / c3 full44 | 1757529, 1757164 | finishing; informational |
| Baseline reruns | (A) 1757931, (B) 1757933; governor 1757996 (600 s; hold >750 G, release <700 G) | running |

## Verification authorities

| Scope | Disposition |
|---|---|
| Intermediate b26 | Release `b26efc0f…`; Debug 353/353; fails WV2–89; full44 2/44 verified (147, 401). Reference only |
| Prior cumulative fuzz | Claude 4d8f46d8 [terminal report](benchmarks/runs/20261007-owner-policy-cumulative-fuzz/FINAL_REPORT.md): 44,000 broad cases, zero failures, targeted reach incomplete. No current-source acceptance |
| Explicit long64-fixed checker | Claude 49c00a94 [ACCEPTED](benchmarks/runs/20261007-long64-fixed-independent-review/DISPOSITION.md) for bounded explicit-profile use; final Trestle verification is mandatory |
| Native proof transport | [Rejected r9](benchmarks/runs/20261007-native-transport-claude-oracle-owner/requalification-deferral-r9/DISPOSITION.md); use pure Python |

## Target investigations and decisions

- **Rank401:** FirstOpen/original-seed repair `0a15ecaf1` has [matched evidence](benchmarks/reports/2026-10-07-firstopen-original-seed-repair/README.md);
  exact b26 checks SAT42.273s. This is target evidence, not cumulative acceptance
  of the new unit candidate. Keep X17 and noL-specific local search retired;
  investigate ordinary Boolean search against actual Satsuma+Kissat order.
- **Rank899 (`mp1-Nb7T42`):** the [certified-unit gate](benchmarks/reports/2026-10-07-rank899-certified-unit-gate/README.md)
  passes. The large-domain producer publishes `+40310`, the proof prefix verifies, the boundary equals
  the Satsuma residual, and startup FastEliminate is suppressed. In a same-CPU pair, d576 is SAT in 55.921 s with a checked model; b26 is UNKNOWN at 300 s.
  This is a causal single-row result. Closure requires c1's full44 row.
- **Rank569:** [affine audit](benchmarks/reports/2026-10-07-rank569-affine-admission-lifecycle-audit/README.md)
  confirms24 recognized, disjoint XOR rows. The [conditional-clause experiment](benchmarks/reports/2026-10-07-rank569-conditional-clause-causal60/README.md)
  is retired for no measured benefit. The3627.980s/4.116GB run remains unverified
  and misses the compact/sub3359.975s target; its deleted proof prevents direct
  confirmation of the checker-failure attribution. Raising matrix budgets alone
  is unsupported. Persistent XOR remains restart-root-only with retained provenance.
- **Rank991 and other owners:** graph construction/Window remains opt-in;
  structural seeding stays off. Apply the [generality gate](DEVELOPMENT_PLAN.md#95-organic-winner-technique-integration)
  to current and proposed LymphoSAT techniques. The [Lucky10 correction](benchmarks/reports/2026-10-07-lucky10-real-input-followup.md)
  preserves21 real recognized rows,11 checked SATs and10 search-free parity declines;
  its admission/cost review remains open. Cheap or unused theory consequences
  do not establish useful integration.

## Standing references and operations

- Keep the approved no-MAB baseline `d170b4a5` and [saved five-arm comparison](benchmarks/reports/2026-10-01-tierd-five-arm-terminal-results.md);
  do not rerun the baseline. Historical accepted gap coverage remains4/44 on its
  [frozen checkpoint](benchmarks/reports/2026-10-07-0e-four-regressions-terminal/README.md),
  not on the current repaired candidate.
- The noL-free checkpoint's [fourth regression gate](benchmarks/reports/2026-10-07-nol-free-wv2-accepted/README.md)
  completes its earlier four-set acceptance; later behavior changes require fresh
  gates. Plan §8.4 owns the exact regression ceilings and bounded checker policy;
  formal full-verification targets remain distinct from regression censors.
- The latest recorded [Tier-B paired window](benchmarks/runs/20260926-163543-B-am466-split-paired/) predates the no-MAB baseline and is historical diagnostic evidence only.
- Follow plan §8.1 for storage and Slurm. Last handoff quota:508GB; earlier audited
  obsolete Debug cleanup reclaimed44.57GB allocated. Recheck before builds and
  clean disposable storage aggressively above700GB. Protect live pins and evidence.
- [Andrew's handoff report](report-to-andrew/20261007T224148Z-session-handoff/report.pdf)
  has adjacent TeX; the [documentation cleanup report](report-to-andrew/20261007T225802Z-documentation-cleanup/report.pdf) records this follow-up. Commits are local; prior automatic
  approval review rejected pushing beyond the previously authorized range.
- Earlier experiment narratives remain in their primary reports and Git. Old
  handoffs and reports describe their own snapshots; they do not authorize
  restoring retired mechanisms or restarting superseded jobs.

## Recent changes

- `d576abaeb`: remove policy suppression of certified symmetry units; validation pending.
- `06b63069b`: record intermediate b26 full Debug353/353, zero skips.
- `5c6e653f7`: record Rank899's exact selected search-prefix match with Kissat.
- `4c17dee25`: add explicit long64-fixed checker profile; independent review accepts it.
- `f4b596b9b`: make generality an integration gate; `e3eb95c84` corrects Lucky10 reach evidence.
